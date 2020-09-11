
import sys
import select
import numpy as np
from scipy.fft import rfft, rfftfreq

from pyqtgraph.Qt import QtGui, QtCore
import pyqtgraph as pg

import signal
signal.signal(signal.SIGINT, signal.SIG_DFL)


NUM_SETS = 50
RANGE = 10e3

ZOOM = (None, None)
if len(sys.argv) == 3:
    ZOOM = float(sys.argv[1]), float(sys.argv[2])
print(ZOOM)

app = QtGui.QApplication([])

window = pg.GraphicsLayoutWidget()
window.setWindowTitle('Fourier transform')

plot_waterfall1 = pg.ImageItem()
p1 = window.addPlot(row=0, col=0)
p1.addItem(plot_waterfall1)

hist = pg.HistogramLUTItem()
hist.setImageItem(plot_waterfall1)
hist.gradient.loadPreset('viridis')
window.addItem(hist, row=0, col=1)

plot_waterfall2 = pg.ImageItem()
p2 = window.addPlot(row=0, col=2)
p2.addItem(plot_waterfall2)

hist = pg.HistogramLUTItem()
hist.setImageItem(plot_waterfall2)
hist.gradient.loadPreset('viridis')
window.addItem(hist, row=0, col=3)

plot_fft1 = window.addPlot(row=1, col=0)
plot_fft2 = window.addPlot(row=1, col=2)

plot_signal1 = window.addPlot(row=2, col=0)
plot_signal2 = window.addPlot(row=2, col=2)

# Global data arrays
samplerate = None
data = None
ffts = None

ts = None
fourierbins = None

LO_CUT = 2
HI_FREQ = 100e3
HI_CUT = None


def process_data(signal1, signal2):
    global samplerate, data, ffts

    n = len(signal1)
    if data is None:
        # First call, init according to data received
        global ts, fourierbins, HI_CUT
        fourierbins = rfftfreq(n, 1/samplerate)[2:]
        fourierbins = fourierbins[fourierbins <= HI_FREQ]
        HI_CUT = LO_CUT + len(fourierbins)

        ts = np.arange(n) / samplerate
        data = np.zeros((NUM_SETS, 2, n))
        ffts = np.zeros((NUM_SETS, 2, len(fourierbins)))

    # shift data
    data = np.roll(data, -1, axis=0)
    ffts = np.roll(ffts, -1, axis=0)

    # store and transform new dataset
    data[-1, 0, :] = signal1
    data[-1, 1, :] = signal2
    ffts[-1, 0, :] = np.log10(np.absolute(rfft(signal1)[LO_CUT:HI_CUT]))
    ffts[-1, 1, :] = np.log10(np.absolute(rfft(signal2)[LO_CUT:HI_CUT]))


def select_subimage(fs, fft, at, range=RANGE):
    if at is None:
        return fs, fft
    m = (at-range < fs) & (fs < at+range)
    return fs[m], fft[:, m]


def update():
    global samplerate, ffts
    global plot_waterfall1, plot_fft1
    global plot_waterfall2, plot_fft2
    global plot_signal1, plot_signal2

    if select.select([sys.stdin], [], [], 0)[0]:
        # New data at stdin
        line1 = sys.stdin.readline()
        line2 = sys.stdin.readline()
        if len(line1) != len(line2):
            sys.stderr.write('#')
        values1 = [float(x) for x in line1.split()]
        values2 = [float(x) for x in line2.split()]
        if values1[1] != 1:
            sys.stderr.write(' async ')
            values1, line1 = values2, line1
            line2 = sys.stdin.readline()
            values2 = [float(x) for x in line2.split()]
        assert values1[1] == 1, values1[1]
        assert values2[1] == 2, values2[1]
        assert values1[0] == values2[0]
        if samplerate is None:
            samplerate = values1[0]
            sys.stderr.write(f"\nSamplerate: {samplerate/1e6:.3f} Msps\n")
        else:
            assert values1[0] == samplerate, values1[0]

        process_data(np.array(values1[2:]), np.array(values2[2:]))

        subfs, subfft = select_subimage(fourierbins, ffts[:, 0, :], ZOOM[0])
        print(subfs[0], subfs[-1], 0, int(subfs[1]/1e3), NUM_SETS, int(np.ceil(subfs[-1])/1e3))
        plot_waterfall1.setImage(subfft)
        plot_waterfall1.setRect(QtCore.QRect(
            0, int(subfs[1]/1e3), NUM_SETS, int(np.ceil(subfs[-1]-subfs[0])/1e3)))
        plot_fft1.plot(fourierbins/1e3, ffts[-1, 0, :], clear=True)

        plot_waterfall2.setImage(ffts[:, 1, :])
        plot_waterfall2.setRect(QtCore.QRect(0, 0, NUM_SETS, int(fourierbins[-1]/1e3)))
        plot_fft2.plot(fourierbins/1e3, ffts[-1, 1, :], clear=True)

        plot_fft1.setRange(yRange=(0, 4.5))
        plot_fft2.setRange(yRange=(0, 4.5))

        plot_signal1.plot(ts[:3000]/1e3, data[-1, 0, :3000], clear=True)
        plot_signal2.plot(ts[:3000]/1e3, data[-1, 0, :3000], clear=True)

        app.processEvents()


timer = QtCore.QTimer()
timer.timeout.connect(update)
timer.start(10)  # 10 ms interval
window.show()

if __name__ == '__main__':
    if (sys.flags.interactive != 1) or not hasattr(QtCore, 'PYQT_VERSION'):
        QtGui.QApplication.instance().exec_()
