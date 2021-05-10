
import numpy as np
from scipy.signal import find_peaks
from scipy.interpolate import interp1d


def slice_intersect(slices, data=None):
    """Calculate intersection of slices.
    Optionally return data inside intersection (first axis) of every slice."""
    inters = slice(
        max(s.start for s in slices),
        min(s.stop for s in slices))
    assert inters.start <= inters.stop, "No intersection."

    if data:
        interd = [
            trace[inters.start-s.start:inters.stop-s.start, ...]
            for s, trace in zip(slices, data)
        ]
        return inters, interd
    else:
        return inters


def envelope(ts, signal, fexp, peak_height=0.1):
    samplerate = 1 / (ts[1]-ts[0])
    peakidxs = find_peaks(signal, height=peak_height,
                          distance=0.8*samplerate/fexp)[0]
    if len(peakidxs) < 4:
        return None, slice(0, len(ts))

    env = interp1d(ts[peakidxs], signal[peakidxs])
    return env, slice(peakidxs[0], peakidxs[-1])


def gauss_laws(
        l, latticeidxs,
        ts, signals, fs,
        C=20e-9, f0=60e3):
    m = len(signals)
    envs, slices = zip(*[envelope(ts, signals[i], fs[i])
                         for i in range(m)])
    numbers = [
        f0/fs[i] * C/2 * envs[i](ts[slices[i]])**2
        if envs[i] is not None else np.full(ts[slices[i]].size, 0)
        for i in range(m)]

    Gs = np.full((l, 4, ts.size), np.nan, dtype=float)
    for i in range(l):
        try:
            siteidx = latticeidxs.index(2*i)
            Gs[i, 0, slices[siteidx]] = numbers[siteidx]
            Gs[i, 3, slices[siteidx]] = numbers[siteidx]
            if i > 0:
                leftidx = latticeidxs.index(2*i-1)
                s = slices[leftidx]
                Gs[i, 1, s] = numbers[leftidx]
                Gs[i, 3, s] -= (-1)**i * numbers[leftidx]
                Gs[i, 3, :s.start] = np.nan
                Gs[i, 3, s.stop:] = np.nan
            if i < l-1:
                rightidx = latticeidxs.index(2*i+1)
                s = slices[rightidx]
                Gs[i, 2, s] = numbers[rightidx]
                Gs[i, 3, s] -= (-1)**i * numbers[rightidx]
                Gs[i, 3, :s.start] = np.nan
                Gs[i, 3, s.stop:] = np.nan
        except ValueError:
            Gs[i, 3] = np.nan

    return Gs
