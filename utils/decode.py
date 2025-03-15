import argparse
from pathlib import Path
import logging

import scipy.io.wavfile as wavfile
from scipy import signal
import numpy as np
import sys
import ldpc

logger = logging.getLogger()

FT8_NUM_TONES = 8
FT8_NUM_SYMBOLS = 79
FT8_TONE_DEVIATION = 6.25
FT8_SYMBOL_PERIOD = 0.160
FT8_SYNC_SYMS = [3, 1, 4, 0, 6, 5, 2]
FT8_SYNC_POS = [0, 36, 72]
FT8_DATA_POS = [7, 43]
FT8_LDPC_PAYLOAD_BITS = 91
FT8_PAYLOAD_BITS = 77

MIN_FREQ = 300
MAX_FREQ = 3000


def lin_to_db(x, eps=1e-12):
    return 20 * np.log10(x + eps)


def db_to_lin(x):
    return 10 ** (x / 20)


def load_wav(path):
    rate, samples = wavfile.read(path)
    if samples.dtype == np.int16:
        samples = np.array(samples / 32768.0)
    return (rate, samples)


def quantize(H, mag_db_step=0.5, phase_divs=256):
    mag_db = lin_to_db(np.abs(H))
    mag_db = mag_db_step * np.ceil(mag_db / mag_db_step)

    phase = np.angle(H)
    phase = np.ceil(0.5 + phase * phase_divs / (2 * np.pi)) / phase_divs * (2 * np.pi)

    return db_to_lin(mag_db) * np.exp(1j * phase)


class Waterfall:
    def __init__(self, freq_osr=2, time_osr=2, freq_min=300, freq_max=3000):
        self.H = None
        self.freq_osr = freq_osr
        self.time_osr = time_osr
        self.window_type = "hann"
        self.freq_step = (
            FT8_TONE_DEVIATION / self.freq_osr
        )  # frequency step corresponding to one bin, Hz
        self.time_step = (
            FT8_SYMBOL_PERIOD / self.time_osr
        )  # time step corresponding to one STFT position, seconds
        self.bin_min = int(freq_min / self.freq_step)
        self.bin_max = int(freq_max / self.freq_step) + 1
        # self.freq_first = self.bin_min * self.freq_step
        # self.time_first = FT8_SYMBOL_PERIOD * self.freq_osr / 2

    def load_signal(self, sig, fs):
        sym_size = int(fs * FT8_SYMBOL_PERIOD)
        nfft = sym_size * self.freq_osr
        _, _, H = signal.stft(
            sig,
            window=self.window_type,
            nfft=nfft,
            nperseg=nfft,
            noverlap=nfft - (sym_size // self.time_osr),
            boundary=None,
            padded=None,
        )
        self.H = quantize(H)
        A = np.abs(H)
        self.Apow = A**2
        self.Adb = lin_to_db(A)
        print(
            f"Max magnitude {self.Adb[:, self.bin_min:self.bin_max].max(axis=(0, 1)):.1f} dB"
        )
        print(f"Waterfall shape {H.shape}")


def search_sync_coarse(wf, min_score=2.5, max_cand=30, snr_mode=2):
    logger.info(f"Using bins {wf.bin_min}..{wf.bin_max} ({wf.bin_max - wf.bin_min})")
    score_map = dict()
    for freq_sub in range(wf.freq_osr):
        for bin_first in range(
            wf.bin_min + freq_sub, wf.bin_max - FT8_NUM_TONES * wf.freq_osr, wf.freq_osr
        ):
            for time_sub in range(wf.time_osr):
                for time_start in range(
                    -10 * wf.time_osr + time_sub,
                    21 * wf.time_osr + time_sub,
                    wf.time_osr,
                ):
                    # calc sync score at (bin_first, time_start)
                    score = []
                    snr_sig = snr_noise = 0
                    for sync_start in FT8_SYNC_POS:
                        for sync_pos, sync_tone in enumerate(
                            FT8_SYNC_SYMS, start=sync_start
                        ):
                            pos = time_start + sync_pos * wf.time_osr
                            if pos >= 0 and pos < wf.Adb.shape[1]:
                                if snr_mode == 0:
                                    snr_sig += wf.Apow[
                                        bin_first + sync_tone * wf.freq_osr, pos
                                    ]
                                    for noise_tone in range(7):
                                        if noise_tone != sync_tone:
                                            snr_noise += wf.Apow[
                                                bin_first + noise_tone * wf.freq_osr,
                                                pos,
                                            ]
                                else:
                                    sym_db = wf.Adb[
                                        bin_first + sync_tone * wf.freq_osr, pos
                                    ]
                                    if (
                                        bin_first + (sync_tone - 1) * wf.freq_osr
                                        >= wf.bin_min
                                    ):
                                        sym_down_db = wf.Adb[
                                            bin_first + (sync_tone - 1) * wf.freq_osr,
                                            pos,
                                        ]
                                        score.append(sym_db - sym_down_db)
                                    if (
                                        bin_first + (sync_tone + 1) * wf.freq_osr
                                        < wf.bin_max
                                    ):
                                        sym_up_db = wf.Adb[
                                            bin_first + (sync_tone + 1) * wf.freq_osr,
                                            pos,
                                        ]
                                        score.append(sym_db - sym_up_db)
                                    if snr_mode == 2:
                                        if pos - 1 >= 0:
                                            sym_prev_db = wf.Adb[
                                                bin_first + sync_tone * wf.freq_osr,
                                                pos - 1,
                                            ]
                                            score.append(sym_db - sym_prev_db)
                                        if pos + 1 < wf.Adb.shape[1]:
                                            sym_next_db = wf.Adb[
                                                bin_first + sync_tone * wf.freq_osr,
                                                pos + 1,
                                            ]
                                            score.append(sym_db - sym_next_db)
                    if snr_mode == 0:
                        score_avg = 10 * np.log10(snr_sig / (snr_noise / 6))
                    else:
                        score_avg = np.mean(score)

                    if score_avg > min_score:
                        is_better = True
                        # if (bin_first, time_start) in score_map:
                        #     if score_map[(bin_first, time_start)] >= score_avg:
                        #         is_better = False
                        for delta_bin in [-2, -1, 0, 1, 2]:
                            for delta_pos in [-2, -1, 0, 1, 2]:
                                key = (bin_first + delta_bin, time_start + delta_pos)
                                if key in score_map:
                                    if score_map[key] <= score_avg:
                                        del score_map[key]
                                    else:
                                        is_better = False
                        if is_better:
                            score_map[(bin_first, time_start)] = score_avg

    top_keys = sorted(score_map.keys(), key=lambda x: score_map[x], reverse=True)[
        :max_cand
    ]
    for idx, (bin, pos) in enumerate(sorted(top_keys)):
        logger.debug(
            f"{idx+1}: {wf.freq_step * bin:.2f}\t{wf.time_step * pos:+.02f}\t{score_map[(bin, pos)]:.2f}"
        )
    time_offset = FT8_SYMBOL_PERIOD / 4
    return [
        (wf.freq_step * bin, wf.time_step * pos - time_offset)
        for (bin, pos) in sorted(top_keys)
    ]


def downsample_fft(H, bin_f0, fs2=100, freq_osr=1, time_osr=1, taper_width=1):
    sym_size2 = int(fs2 * FT8_SYMBOL_PERIOD)
    nfft2 = sym_size2 * freq_osr
    freq_step2 = fs2 / nfft2
    pad_width = (nfft2 - 2 * taper_width - freq_osr * FT8_NUM_TONES) // 2
    H2 = H[
        bin_f0 - taper_width - pad_width : bin_f0
        + freq_osr * FT8_NUM_TONES
        + taper_width
        + pad_width,
        :,
    ]
    W_taper = np.linspace(0, 1, taper_width)
    W_pad = [0] * pad_width
    W = np.concatenate(
        (W_pad, W_taper, [1] * freq_osr * FT8_NUM_TONES, np.flipud(W_taper), W_pad)
    )
    H2 = np.multiply(H2, np.expand_dims(W, W.ndim))

    shift = taper_width + pad_width
    H2 = np.roll(H2, -shift, axis=0)
    _, sig_down = signal.istft(
        H2,
        window="hann",
        nperseg=nfft2,
        noverlap=nfft2 - (sym_size2 // time_osr),
        input_onesided=False,
    )

    f0_down = (taper_width + pad_width - shift) * freq_step2
    return sig_down, f0_down


def search_sync_fine(sig_down, fs2, f0_down, pos_start):
    sym_size2 = int(fs2 * FT8_SYMBOL_PERIOD)
    n = np.arange(sym_size2)

    f_tones = np.arange(
        f0_down, f0_down + FT8_NUM_TONES * FT8_TONE_DEVIATION, FT8_TONE_DEVIATION
    )
    ctones_conj = np.exp(
        -1j * 2 * np.pi * np.expand_dims(n, n.ndim) * np.expand_dims(f_tones / fs2, 0)
    )
    ctweak_plus_tone = np.exp(-1j * 2 * np.pi * n * FT8_TONE_DEVIATION / fs2)
    ctweak_minus_tone = np.exp(1j * 2 * np.pi * n * FT8_TONE_DEVIATION / fs2)

    max_power, fine_freq_offset, fine_pos_offset = None, None, None
    all_powers = []
    win = signal.windows.kaiser(sym_size2, beta=2.0)
    # win = signal.windows.boxcar(sym_size2)
    for freq_offset in np.linspace(-3.2, 3.2, 33):
        power_time = []
        ctweak = np.exp(-1j * 2 * np.pi * n * freq_offset / fs2)
        for pos_offset in range(-sym_size2 // 2, sym_size2 // 2 + 1):
            power_sig = 0
            power_nse = 1e-12
            for sync_start in FT8_SYNC_POS:
                for sync_pos, sync_tone in enumerate(FT8_SYNC_SYMS):
                    pos1 = pos_start + pos_offset + sym_size2 * (sync_start + sync_pos)

                    if pos1 >= 0 and pos1 + sym_size2 < len(sig_down):
                        demod = (
                            win
                            * sig_down[pos1 : pos1 + sym_size2]
                            * ctones_conj[:, sync_tone]
                            * ctweak
                            * np.exp(-1j * 2 * np.pi * pos1 * freq_offset / fs2)
                        )
                        mag2_sym = np.abs(np.sum(demod)) ** 2
                        mag2_minus = np.abs(np.sum(demod * ctweak_minus_tone)) ** 2
                        mag2_plus = np.abs(np.sum(demod * ctweak_plus_tone)) ** 2
                        power_sig += mag2_sym
                        power_nse += (mag2_minus + mag2_plus) / 2

                        # demod_prev = win * sig_down[pos1 - sym_size2:pos1] * ctones_conj[:, sync_tone] * ctweak
                        # demod_next = win * sig_down[pos1 + sym_size2:pos1 + 2*sym_size2] * ctones_conj[:, sync_tone] * ctweak
                        # mag2_prev = np.abs(np.sum(demod_prev))**2
                        # mag2_next = np.abs(np.sum(demod_next))**2
                        # power += 2*mag2_sym - mag2_prev - mag2_next
            # power = lin_to_db(power_sig / power_nse)/2
            power = power_sig / power_nse
            power_time.append(power)
            if max_power is None or power > max_power:
                max_power = power
                fine_freq_offset = freq_offset
                fine_pos_offset = pos_offset

        # print(f'{freq_offset:.1f}, {(np.argmax(power_time) - sym_size2//2)/fs2:.3f}, {np.max(power_time)}')
        all_powers.append(power_time)

    return fine_freq_offset, fine_pos_offset


def extract_logl_db(A2db):
    # FT8 bits -> channel symbols 0, 1, 3, 2, 5, 6, 4, 7
    A2db_bit0 = np.max(A2db[[5, 6, 4, 7], :], axis=0) - np.max(
        A2db[[0, 1, 3, 2], :], axis=0
    )  # 4/5/6/7 - 0/1/2/3
    A2db_bit1 = np.max(A2db[[3, 2, 4, 7], :], axis=0) - np.max(
        A2db[[0, 1, 5, 6], :], axis=0
    )  # 2/3/6/7 - 0/1/4/5
    A2db_bit2 = np.max(A2db[[1, 2, 6, 7], :], axis=0) - np.max(
        A2db[[0, 3, 5, 4], :], axis=0
    )  # 1/3/5/7 - 0/2/4/6
    A2db_bits = np.stack((A2db_bit0, A2db_bit1, A2db_bit2)).transpose()

    # a = [
    #     A2db[7, :] - A2db[0, :],
    #     A2db[3, :] - A2db[0, :],
    #     A2db[6, :] - A2db[3, :],
    #     A2db[6, :] - A2db[2, :],
    #     A2db[7, :] - A2db[4, :],
    #     A2db[4, :] - A2db[1, :],
    #     A2db[5, :] - A2db[1, :],
    #     A2db[5, :] - A2db[2, :]
    # ]
    # W = np.array([[ 48.,   6.,  36.,  30.,   6.,  36.,  30.,  24.],
    #  [ 42.,  35., -28., -29.,   1.,  40.,   5., -30.],
    #  [ 42.,   1.,  40.,   5.,  35., -28., -29., -30.]])/34/6
    # A2db_bits = np.matmul(W, a).transpose()

    bits_logl = np.concatenate((A2db_bits[7:36], A2db_bits[43:72])).flatten()
    return 1.0 * bits_logl, A2db_bits


def show_plots(A2db, A2db_bits, bits_logl):
    import matplotlib.pyplot as plt

    # import matplotlib.ticker as plticker
    import matplotlib.colors as pltcolors

    fig, ax = plt.subplots(4, figsize=(10.0, 8.0))

    plt.colorbar(
        ax[0].imshow(
            A2db,
            cmap="inferno",
            norm=pltcolors.Normalize(-30, 0, clip=True),
            origin="lower",
        ),
        orientation="horizontal",
        ax=ax[0],
    )
    plt.colorbar(
        ax[1].imshow(
            A2db_bits.transpose(),
            cmap="bwr",
            norm=pltcolors.Normalize(-10, 10, clip=True),
            origin="lower",
        ),
        orientation="horizontal",
        ax=ax[1],
    )
    # ax[2].imshow(A2db_bits2, cmap='bwr', norm=pltcolors.Normalize(-10, 10, clip=True))
    ax[2].hist(bits_logl, bins=25)
    # ax[3].plot(np.arange(len(sig3))/sym_size2, np.real(sig3))
    # ax[3].plot(np.arange(len(sig3))/sym_size2, np.abs(sig3))
    ax[3].margins(0, 0)
    # loc = plticker.MultipleLocator(base=32.0) # this locator puts ticks at regular intervals
    # ax[1].xaxis.set_major_locator(loc)
    # ax[0].plot(np.array(all_powers).transpose())

    plt.grid()
    plt.show()


def main(wave, downsample, time, freq, noise, freq_osr=2, time_osr=2):
    fs, sig = load_wav(wave)
    logger.info(f"Sample rate: {fs} Hz")

    if noise is not None:
        sig_rms = np.std(sig)
        logger.info(f"Signal RMS: {sig_rms}")
        np.random.seed(1337)
        sig += np.random.normal(0, sig_rms * np.pow(10, noise / 20), len(sig))

    wf = Waterfall(
        freq_osr=freq_osr, time_osr=time_osr, freq_min=MIN_FREQ, freq_max=MAX_FREQ
    )
    wf.load_signal(sig, fs)

    if freq is not None and time is not None:
        candidates = [(freq, time)]
    else:
        candidates = search_sync_coarse(wf, snr_mode=1, max_cand=50)

    logger.info(f"Coarse candidates: {len(candidates)}")

    num_decoded = 0
    for f0, time_start in candidates:
        bin_f0 = int(0.5 + f0 / wf.freq_step)
        f0_coarse = bin_f0 * wf.freq_step

        if downsample:
            fs2 = 100
            # env_alpha = 0.06

            sig_down, f0_down = downsample_fft(
                wf.H[:, ::time_osr], bin_f0, fs2=fs2, freq_osr=freq_osr, time_osr=1
            )

            pos_start = int(0.5 + time_start * fs2)
            fine_freq_offset, fine_pos_offset = search_sync_fine(
                sig_down, fs2, f0_down, pos_start
            )
            f0_down_fine, pos_fine = (
                fine_freq_offset + f0_down,
                pos_start + fine_pos_offset,
            )

            # logger.info(
            #     f"Downsampled signal to {fs2} Hz sample rate, freq shift {f0_coarse} Hz -> {f0_down} Hz"
            # )
            print(
                f"Fine sync at {f0_coarse:.2f} + {fine_freq_offset:.2f} = {f0_coarse + fine_freq_offset:.2f} Hz, {pos_start/fs2:.3f} + {fine_pos_offset/fs2:.3f} = {pos_fine/fs2:.3f} s"
            )

            # env = signal.filtfilt(env_alpha, [1, -(1 - env_alpha)], np.abs(sig_down))

            sym_size2 = int(fs2 * FT8_SYMBOL_PERIOD)
            ctweak = np.exp(
                -1j * 2 * np.pi * np.arange(len(sig_down)) * f0_down_fine / fs2
            )
            slice_pos = pos_start + fine_pos_offset
            slice_length = int(FT8_NUM_SYMBOLS * FT8_SYMBOL_PERIOD * fs2)
            pad_left = pad_right = 0
            if slice_pos < 0:
                pad_left = -slice_pos
                slice_pos = 0
            if slice_pos + slice_length > len(sig_down) + pad_left:
                pad_right = slice_pos + slice_length - (len(sig_down) + pad_left)
            sig3 = np.pad(
                sig_down * ctweak,
                (pad_left, pad_right),
                mode="constant",
                constant_values=(0, 0),
            )[slice_pos : slice_pos + slice_length]

            # win = signal.windows.kaiser(sym_size2, beta=1.6)
            win = signal.windows.boxcar(sym_size2)
            _, _, H2 = signal.stft(
                sig3,
                window=win,
                nperseg=sym_size2,
                noverlap=0,
                return_onesided=False,
                boundary=None,
                padded=False,
            )
            A2db = lin_to_db(np.abs(H2[0:FT8_NUM_TONES, :]))
        else:
            time_offset = FT8_SYMBOL_PERIOD / 4
            pos_start = int(0.5 + (time_start + time_offset) / wf.time_step)
            # print(
            #     f"Start time {time_start:.3f} s (pos {pos_start}), coarse {pos_start * wf.time_step - time_offset:.3f} s"
            # )
            # TODO: zero padding for time axis
            pad_left, pad_right = 0, 0
            if pos_start < 0:
                pad_left = -pos_start
                pos_start = 0
            if pos_start + wf.Adb.shape[1] > FT8_NUM_SYMBOLS * time_osr:
                pad_right = pos_start + wf.Adb.shape[1] - (FT8_NUM_SYMBOLS * time_osr)

            Adb = np.pad(
                wf.Adb,
                (pad_left, pad_right),
                mode="constant",
                constant_values=(0, 0),
            )

            A2db = Adb[
                bin_f0 : bin_f0 + freq_osr * FT8_NUM_TONES : freq_osr,
                pos_start : pos_start + FT8_NUM_SYMBOLS * time_osr : time_osr,
            ]

        A2db -= np.max(A2db, axis=0)

        bits_logl, A2db_bits = extract_logl_db(A2db)
        (bits, num_errors, iters) = ldpc.bp_solve(
            bits_logl, max_iters=30, max_no_improvement=15
        )
        if num_errors == 0:
            logger.info(
                f"Frequency {f0:.2f} Hz (bin {bin_f0}), coarse {f0_coarse:.2f} Hz"
            )

            print(f"LDPC decode: {num_errors} errors, {iters} iterations")

            print(f'Payload bits: {"".join([str(x) for x in bits[:FT8_PAYLOAD_BITS]])}')
            print(
                f'CRC bits    : {"".join([str(x) for x in bits[FT8_PAYLOAD_BITS:FT8_LDPC_PAYLOAD_BITS]])}'
            )
            print(
                f'Parity bits : {"".join([str(x) for x in bits[FT8_LDPC_PAYLOAD_BITS:]])}'
            )
            num_decoded += 1

            show_plots(A2db, A2db_bits, bits_logl)
            break

    logger.info(f"Total decoded: {num_decoded}")
    # show_plots(A2db, A2db_bits, bits_logl)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    parser = argparse.ArgumentParser()
    parser.add_argument("wave", type=Path)
    parser.add_argument("-d", "--downsample", action="store_true")
    parser.add_argument("-t", "--time", nargs="?", type=float)
    parser.add_argument("-f", "--freq", nargs="?", type=float)
    parser.add_argument("-n", "--noise", nargs="?", type=float)
    args = parser.parse_args()

    main(**vars(args))
