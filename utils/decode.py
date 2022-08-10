import scipy.io.wavfile as wavfile
from scipy import signal
import numpy as np
import sys

FT8_NUM_TONES = 8
FT8_NUM_SYMBOLS = 79
FT8_TONE_DEVIATION = 6.25
FT8_SYMBOL_PERIOD = 0.160
FT8_SYNC_SYMS = [3, 1, 4, 0, 6, 5, 2]
FT8_SYNC_POS = [0, 36, 72]

MIN_FREQ = 300
MAX_FREQ = 3000

def lin_to_db(x):
    return 20*np.log10(x + 1e-12)

def db_to_lin(x):
    return 10**(x/20)

def load_wav(path):
    rate, samples = wavfile.read(path)
    if samples.dtype == np.int16:
        samples = np.array(samples / 32768.0)
    return (rate, samples)

class Waterfall:
    def __init__(self):
        pass


def downsample_fft(H, bin_f0, fs2=100, freq_osr=1, time_osr=1):
    sym_size2 = int(fs2 * FT8_SYMBOL_PERIOD)
    nfft2 = sym_size2 * freq_osr
    bin_freq2 = fs2 / nfft2
    taper_width = 6
    pad_width = ((nfft2 - 2*taper_width - freq_osr*FT8_NUM_TONES) // 2)
    H2 = H[bin_f0 - taper_width - pad_width: bin_f0 + freq_osr*FT8_NUM_TONES + taper_width + pad_width, :]
    W_taper = np.linspace(0, 1, taper_width)
    W_pad = [0] * pad_width
    W = np.concatenate( (W_pad, W_taper, [1]*freq_osr*FT8_NUM_TONES, np.flipud(W_taper), W_pad) )
    H2 = np.multiply(H2, np.expand_dims(W, W.ndim))

    shift = taper_width + pad_width
    H2 = np.roll(H2, -shift, axis=0)
    _, sig2 = signal.istft(H2, window='hann', nperseg=nfft2, noverlap=nfft2 - (sym_size2//time_osr), input_onesided=False)

    f0_new = (taper_width + pad_width - shift) * bin_freq2
    return sig2, f0_new


def locate_fine_sync(sig2, fs2, f0_new, pos_start):
    sym_size2 = int(fs2 * FT8_SYMBOL_PERIOD)
    n = np.arange(sym_size2)

    f_tones = np.arange(f0_new, f0_new + FT8_NUM_TONES*FT8_TONE_DEVIATION, FT8_TONE_DEVIATION)
    ctones_conj = np.exp(-1j * 2*np.pi * np.expand_dims(n, n.ndim) * np.expand_dims(f_tones/fs2, 0))
    ctweak_plus_tone = np.exp(-1j * 2*np.pi * n * FT8_TONE_DEVIATION/fs2)
    ctweak_minus_tone = np.exp(1j * 2*np.pi * n * FT8_TONE_DEVIATION/fs2)

    max_power, max_freq_offset, max_pos_offset = None, None, None
    all_powers = []
    win = signal.windows.kaiser(sym_size2, beta=2.0)
    for freq_offset in np.linspace(-2.5, 2.5, 21):
        power_time = []
        ctweak = np.exp(-1j * 2*np.pi * n * freq_offset/fs2)
        for pos_offset in range(-sym_size2//2, sym_size2//2 + 1):
            power = 0
            for sync_start in FT8_SYNC_POS:
                for sync_pos, sync_tone in enumerate(FT8_SYNC_SYMS):
                    pos1 = pos_start + pos_offset + sym_size2 * (sync_start + sync_pos)

                    if pos1 >= 0 and pos1 + sym_size2 < len(sig2):
                        demod = win * sig2[pos1:pos1 + sym_size2] * ctones_conj[:, sync_tone] * ctweak
                        mag2_sym = np.abs(np.sum(demod))**2
                        # power += mag2_sym
                        mag2_minus = np.abs(np.sum(demod * ctweak_minus_tone))**2
                        mag2_plus = np.abs(np.sum(demod * ctweak_plus_tone))**2
                        power += 2*mag2_sym - mag2_minus - mag2_plus

                    # demod_prev = win * sig2[pos1 - sym_size2:pos1] * ctones_conj[:, sync_tone] * ctweak
                    # demod_next = win * sig2[pos1 + sym_size2:pos1 + 2*sym_size2] * ctones_conj[:, sync_tone] * ctweak
                    # mag2_prev = np.abs(np.sum(demod_prev))**2
                    # mag2_next = np.abs(np.sum(demod_next))**2
                    # power += 2*mag2_sym - mag2_prev - mag2_next
            power_time.append(power)
            if max_power is None or power > max_power:
                max_power = power
                max_freq_offset = freq_offset
                max_pos_offset = pos_offset
            
        print(f'{freq_offset:.1f}, {(np.argmax(power_time) - sym_size2//2)/fs2:.3f}, {np.max(power_time)}')
        all_powers.append(power_time)

    return max_freq_offset, max_pos_offset


fs, sig = load_wav(sys.argv[1])
print(f'Sample rate {fs} Hz')

freq_osr = 2
time_osr = 4

sym_size = int(fs * FT8_SYMBOL_PERIOD)
nfft = sym_size * freq_osr
bin_freq = fs / nfft
_, _, H = signal.stft(sig, window='hann', nperseg=nfft, noverlap=nfft - (sym_size//time_osr), boundary=None, padded=None)
Adb = lin_to_db(np.abs(H))
print(f'Max magnitude {Adb.max(axis=(0, 1)):.1f} dB')
print(f'Waterfall shape {Adb.shape}')

# bin_min = int(MIN_FREQ / bin_freq)
# bin_max = int(MAX_FREQ / bin_freq) + 1
# print(f'Using bins {bin_min}..{bin_max} ({bin_max - bin_min})')
# for freq_sub in range(freq_osr):
#     for bin_first in range(bin_min + freq_sub, bin_max - FT8_NUM_TONES, freq_osr):
#         for time_sub in range(time_osr):
#             for time_start in range(-10 + time_sub, 20, time_osr):
#                 # calc sync score at (bin_first, time_start)
#                 pass

f0 = float(sys.argv[2])
bin_f0 = int(0.5 + f0 / bin_freq)
f0_real = bin_f0 * bin_freq

fs2 = 200
sig2, f0_new = downsample_fft(H[:, ::time_osr], bin_f0, fs2=fs2, freq_osr=freq_osr, time_osr=1)
print(f'Downsampled signal to {fs2} Hz sample rate, freq shift {f0_real} Hz -> {f0_new} Hz')

time_start = float(sys.argv[3])
pos_start = int(0.5 + time_start * fs2)

max_freq_offset, max_pos_offset = locate_fine_sync(sig2, fs2, f0_new, pos_start)
print(f'Max power at {f0_real:.1f} + {max_freq_offset:.1f} = {f0_real + max_freq_offset:.1f} Hz, {max_pos_offset/fs2:.3f} s')

env = signal.filtfilt(0.04, [1, -(1-0.04)], np.abs(sig2))
# env = signal.filtfilt(0.01, [1, -(1-0.01)], np.abs(sig2))

# max_freq_offset = f0 - f0_real
# max_pos_offset = 0
sym_size2 = int(fs2 * FT8_SYMBOL_PERIOD)
ctweak = np.exp(-1j * 2*np.pi * np.arange(len(sig2)) * max_freq_offset/fs2)
sig3 = (sig2*ctweak/env)[pos_start + max_pos_offset:pos_start + max_pos_offset + int(FT8_NUM_SYMBOLS*FT8_SYMBOL_PERIOD*fs2)]
_, _, H2 = signal.stft(sig3, window='boxcar', nperseg=sym_size2, noverlap=0, return_onesided=False, boundary=None, padded=False)
A2db = lin_to_db(np.abs(H2))
A2db = A2db[0:8, :]

# FT8 bits -> channel symbols 0, 1, 3, 2, 5, 6, 4, 7
A2db_bit0 = np.max(A2db[[5, 6, 4, 7], :], axis=0) - np.max(A2db[[0, 1, 3, 2], :], axis=0) # 4/5/6/7 - 0/1/2/3
A2db_bit1 = np.max(A2db[[3, 2, 4, 7], :], axis=0) - np.max(A2db[[0, 1, 5, 6], :], axis=0) # 2/3/6/7 - 0/1/4/5
A2db_bit2 = np.max(A2db[[1, 2, 6, 7], :], axis=0) - np.max(A2db[[0, 3, 5, 4], :], axis=0) # 1/3/5/7 - 0/2/4/6
A2db_bits = np.stack((A2db_bit0, A2db_bit1, A2db_bit2))
# a = [
#     A2db[7, :] - A2db[0, :],
#     A2db[3, :] - A2db[0, :],
#     A2db[6, :] - A2db[3, :],
#     A2db[6, :] - A2db[2, :],
#     A2db[7, :] - A2db[4, :],
#     A2db[4, :] - A2db[1, :],
#     A2db[5, :] - A2db[1, :]
# ]
# A2db_bit0a = (a[0] + a[2] + a[3] + a[5] + a[6])/5
# A2db_bit1a = a[0] / 4 + (a[1] - a[3]) * 5 / 24 + (a[5] - a[2]) / 6 + (a[4] - a[6]) / 24
# A2db_bit2a = a[0] / 4 + (a[1] - a[3]) / 24 + (a[2] - a[5]) / 6 + (a[4] - a[6]) * 5 / 24

# 3140652 00000000117217624541127053545 3140652 33170166234757420515470163426 3140652

import matplotlib.pyplot as plt
import matplotlib.ticker as plticker
import matplotlib.colors as pltcolors
fig, ax = plt.subplots(4)

plt.colorbar(ax[0].imshow(A2db, cmap='inferno', norm=pltcolors.Normalize(-30, 0, clip=True)), orientation='horizontal', ax=ax[0])
# ax[2].imshow(np.stack((A2db_bit0a, A2db_bit1a, A2db_bit2a)), cmap='bwr', norm=pltcolors.Normalize(-10, 10, clip=True))
plt.colorbar(ax[1].imshow(A2db_bits, cmap='bwr', norm=pltcolors.Normalize(-10, 10, clip=True)), orientation='horizontal', ax=ax[1])
ax[2].hist(A2db_bits.flatten(), bins=25)
ax[3].plot(np.arange(len(sig3))/sym_size2, np.real(sig3))
ax[3].plot(np.arange(len(sig3))/sym_size2, np.abs(sig3))
ax[3].margins(0, 0)
# loc = plticker.MultipleLocator(base=32.0) # this locator puts ticks at regular intervals
# ax[1].xaxis.set_major_locator(loc)
# ax[0].plot(np.array(all_powers).transpose())

plt.grid()
plt.show()
