import scipy.io.wavfile as wavfile
from scipy import signal
import numpy as np
import sys

FT8_NUM_TONES = 8
FT8_TONE_DEVIATION = 6.25
FT8_SYMBOL_PERIOD = 0.160
MIN_FREQ = 300
MAX_FREQ = 3000

def load_wav(path):
    rate, samples = wavfile.read(path)
    if samples.dtype == np.int16:
        samples = np.array(samples / 32768.0)
    return (rate, samples)

fs, sig = load_wav(sys.argv[1])
print(f'Sample rate {fs} Hz')

sym_size = int(fs * FT8_SYMBOL_PERIOD)
nfft = 2 * sym_size
bin_freq = fs / nfft
_, _, H = signal.stft(sig, window='hann', nperseg=nfft, noverlap=nfft - (sym_size))
A = np.abs(H)
Adb = 20 * np.log10(A + 1e-12)
print(f'Max magnitude {Adb.max(axis=(0, 1)):.1f} dB')
print(f'Waterfall shape {Adb.shape}')

bin_min = int(MIN_FREQ / bin_freq)
bin_max = int(MAX_FREQ / bin_freq) + 1
print(f'Using bins {bin_min}..{bin_max} ({bin_max - bin_min})')

# for freq_osr in range(2):
#     for bin_first in range(bin_min + freq_osr, bin_max - FT8_NUM_TONES, 2):
#         for time_osr in range(2):
#             for time_start in range(-10 + time_osr, 20, 2):
#                 # calc sync score at (bin_first, time_start)
#                 pass

f0 = float(sys.argv[2])
bin_f0 = int(f0 / bin_freq)

fs2 = 100
sym_size2 = int(fs2 * FT8_SYMBOL_PERIOD)
nfft2 = sym_size2 * 2
H2 = H[bin_f0 - 8: bin_f0 + 2*FT8_NUM_TONES + 8, :]
W = np.array([[0, 0.1, 0.2, 0.4, 0.6, 0.8, 0.9, 1] + [1]*2*FT8_NUM_TONES + [1, 0.9, 0.8, 0.6, 0.4, 0.2, 0.1, 0]])
# print(H2.shape, W.shape, np.multiply(H2, W.transpose()))
_, sig2 = signal.istft(np.multiply(H2, W.transpose()), window=[1]*nfft2, nperseg=nfft2, noverlap=nfft2 - (sym_size2), input_onesided=False)
print(sig2.shape)

import matplotlib.pyplot as plt
plt.plot(np.imag(sig2[400:1000]))
plt.show()