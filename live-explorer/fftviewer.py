"""Usage: python fftviewer.py IP1=IP2=IP3 FREQ1 FREQ2 FREQ3...

As command line arguments supply in the first argument all IPs of the
Red Pitayas separated by `=`.  In the following arguments specify
center frequencies for the Fourier trafo plots.
"""

import sys
import numpy as np
from scipy.fft import rfft, rfftfreq
import scipy.signal.windows as windows

from matplotlib import cm as colormaps
from pyqtgraph.Qt import QtGui, QtCore
import pyqtgraph as pg

from rpchain import RPChain, RingBufferChain, RPBUFFERSIZE
from gauss_laws import gauss_laws

import signal
signal.signal(signal.SIGINT, signal.SIG_DFL)


# Width and length of waterfall plot
FWIDTH = 12e3  # Hz
WATERFALL_LENGTH = 100
VMIN, VMAX = -4, 0.1

# Limit number of samples in third row
SAMPLES_LEN = (2**14-210)
INIT_SAMPLE = 200

# Limit Fourier trafo.  These are also the x limits for
# the spectra in the second row.
F_LO_CUT = 2  # samples
F_HI_FREQ = 100e3  # Hz

# Samplerate of Red Pitaya
SAMPLERATE = 125e6 / 64


rpips = sys.argv[1].split('=')
fcenter = [float(fc) for fc in sys.argv[2:]]
print(f"{len(rpips)} Red Pitayas:", rpips)
print(f"Frequencies for {len(fcenter)} channels specfied:", fcenter)

nchannels = 2 * len(rpips)
assert len(fcenter) == 0 or len(fcenter) == 2*len(rpips)
if len(fcenter) == 0:
    fcenter = [50e3]*nchannels

chain = RPChain()
chain.connect(rpips)
print("Connected.")

# Do a general upper cut-off to limit amount of data in diagrams.
fbins = rfftfreq(RPBUFFERSIZE-INIT_SAMPLE, 1/SAMPLERATE)[F_LO_CUT:]
fbins = fbins[fbins <= F_HI_FREQ]
F_HI_CUT = F_LO_CUT + len(fbins)
ts = (np.arange(RPBUFFERSIZE)-INIT_SAMPLE) / SAMPLERATE


def buffertrafo(values):
    win = windows.hamming(RPBUFFERSIZE-INIT_SAMPLE)
    fft = rfft(values[INIT_SAMPLE:] * win) / len(values)
    return np.log10(np.absolute(fft[F_LO_CUT:F_HI_CUT]))


ringbuffer = RingBufferChain(
    chain, WATERFALL_LENGTH,
    transform=buffertrafo,
    fill=np.nan,
    nvalues=F_HI_CUT-F_LO_CUT)

print("Starting GUI")
app = QtGui.QApplication([])
window = pg.GraphicsLayoutWidget()
window.setWindowTitle("RP Live Explorer")

waterfalls = []
ffts = []
signals = []
gausslaws = []
for i in range(nchannels):
    waterfall = pg.ImageItem()
    plot = window.addPlot(row=0, col=2*i)
    plot.addItem(waterfall)
    plot.showGrid(y=True, alpha=1)
    waterfalls.append(waterfall)

    # https://github.com/pyqtgraph/pyqtgraph/issues/561#issuecomment-329904839
    colormap = colormaps.get_cmap('viridis')
    colormap._init()
    lut = (colormap._lut * 255).view(np.ndarray)
    waterfall.setLookupTable(lut)

    #hist = pg.HistogramLUTItem()
    #hist.setImageItem(waterfall)
    #hist.gradient.loadPreset('viridis')
    #window.addItem(hist, row=0, col=2*i+1)

    # https://stackoverflow.com/a/46605797/13355370
    for key in plot.axes:
        plot.getAxis(key).setZValue(1)

    fft = window.addPlot(row=1, col=2*i)
    fft.showGrid(x=True, y=True, alpha=0.8)
    ffts.append(fft)

    signal = window.addPlot(row=2, col=2*i)
    signals.append(signal)

for i in range(nchannels//2):
    law = window.addPlot(row=3, col=4*i, colspan=3)
    law.showGrid(y=True, x=True, alpha=0.8)
    gausslaws.append(law)
gausslawpens = [
    pg.mkPen('b'),
    pg.mkPen('g'),
    pg.mkPen('y'),
    pg.mkPen('r')
]

pensites = pg.mkPen('#3871c1')
penlinks = pg.mkPen('#f68712')


def select_subimage(fbins, spectrum, fcenter, fwidth=FWIDTH):
    if fcenter is None or np.isnan(fcenter):
        return fbins, spectrum
    m = (fcenter-fwidth/2 < fbins) & (fbins < fcenter+fwidth/2)
    return fbins[m], spectrum[:, m]


def update():
    if ringbuffer.read(0.01):
        for i, fc in enumerate(fcenter):
            subfbins, subspectrum = select_subimage(
                fbins, ringbuffer.transformed[i], fc)
            waterfalls[i].setImage(subspectrum, levels=(VMIN, VMAX))
            waterfalls[i].setRect(QtCore.QRectF(
                0, -FWIDTH/2/1e3, WATERFALL_LENGTH, FWIDTH/1e3))

            ffts[i].plot(fbins/1e3, ringbuffer.transformed[i, 0], clear=True)
            ffts[i].setRange(xRange=((fc-FWIDTH/2)/1e3, (fc+FWIDTH/2)/1e3))

            signals[i].plot(
                ts[:SAMPLES_LEN]*1e3, ringbuffer.buffer[i, 0, :SAMPLES_LEN],
                clear=True, pen=(pensites if i % 2 == 0 else penlinks))
            signals[i].setRange(yRange=(-1.2, 1.2))

        Gs = gauss_laws(
            5, [0, 1, 2, 3, 4, 5, 6, 7, 8],
            ts[:SAMPLES_LEN], ringbuffer.buffer[:, 0, :SAMPLES_LEN], fcenter)
        for i in range(len(gausslaws)):
            for j in range(Gs.shape[1]):
                gausslaws[i].plot(
                    ts[:SAMPLES_LEN]*1e3, Gs[i, j]*1e9,
                    pen=gausslawpens[j], clear=(j == 0))
            gausslaws[i].setRange(xRange=(ts[0]*1e3, ts[:SAMPLES_LEN][-1]*1e3))

    app.processEvents()


timer = QtCore.QTimer()
timer.timeout.connect(update)
timer.start(10)  # 10 ms interval
window.show()

if __name__ == '__main__':
    if (sys.flags.interactive != 1) or not hasattr(QtCore, 'PYQT_VERSION'):
        QtGui.QApplication.instance().exec_()
