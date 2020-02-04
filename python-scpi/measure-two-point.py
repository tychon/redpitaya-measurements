import sys
import time
import visa

import numpy as np
import matplotlib.pyplot as plt

TCP_HOST = '169.254.70.102' if len(sys.argv) < 2 else sys.argv[1]
TCP_PORT = 5000

rm = visa.ResourceManager('@py')
inst = rm.open_resource(f'TCPIP0::{TCP_HOST}::{TCP_PORT}::SOCKET')
inst.write_termination = '\r\n'
inst.read_termination = '\r\n'

BUFFER_SIZE = 2**14  # samples
BASE_SAMPLERATE = 125e6  # samples per second
DECIMATIONS = np.array([1, 8, 64, 1024, 8192, 65536])

RESISTANCE = 33  # Ohm


def acquire_data(f, amp=1, delay=8192, inst=inst, quiet=False):
    # Choose decimation factor where f is oversampled by just about a
    # factor of 20.
    targetdecimation = BASE_SAMPLERATE / (f * 100)
    decimation = DECIMATIONS[DECIMATIONS <= targetdecimation].max(initial=1)
    buffertime = BUFFER_SIZE / (BASE_SAMPLERATE / decimation)  # s
    ncyc = 10 + int(f * buffertime)
    #print(f, targetdecimation, decimation, buffertime, ncyc)

    inst.write('GEN:RST')  # reset function generator
    inst.write('ACQ:RST')  # reset analog input
    inst.write('DIG:PIN LED0,1')
    # setup function generator
    inst.write('SOUR1:FUNC SINE')
    inst.write('SOUR1:FREQ:FIX {:f}'.format(f))
    inst.write('SOUR1:VOLT {:f}'.format(amp))
    # inst.write('SOUR1:BURS:STAT OFF')
    # print(inst.query('*IDN?'))
    inst.write('SOUR1:BURS:NCYC {:d}'.format(ncyc))
    inst.write('OUTPUT1:STATE ON')
    # setup oscilloscope
    inst.write('ACQ:DEC {:f}'.format(decimation))
    inst.write('ACQ:TRIG:LEV {:f}'.format(amp/10))
    inst.write('ACQ:TRIG:DLY {:d}'.format(delay))
    inst.write('ACQ:START')
    # wait for buffer to fill
    if BUFFER_SIZE/2 - delay > 0:
        time.sleep(1.1 * (BUFFER_SIZE/2 - delay)
                   / (BASE_SAMPLERATE / decimation))
    inst.write('DIG:PIN LED0,0')
    inst.write('DIG:PIN LED1,1')

    # trigger from function generator
    inst.write('ACQ:TRIG AWG_PE')
    inst.write('SOUR1:TRIG:IMM')

    # wait for trigger
    while True:
        trig_rsp = inst.query('ACQ:TRIG:STAT?')
        if not quiet:
            sys.stdout.write(trig_rsp + ' ')
            sys.stdout.flush()
        if trig_rsp == 'TD':
            if not quiet:
                sys.stdout.write('\n')
            break

    inst.write('DIG:PIN LED1,0')
    inst.write('DIG:PIN LED2,1')

    # read data from IN1
    inst.write('ACQ:SOUR1:DATA?')
    data1 = inst.read()
    assert data1[0] == '{' and data1[-1] == '}'
    data1 = np.array(list(float(v) for v in data1[1:-1].split(',')))

    inst.write('GEN:RST')
    inst.write('ACQ:RST')
    inst.write('DIG:PIN LED2,0')

    return BASE_SAMPLERATE/decimation, data1


def demodulate(signal, f, samplerate):
    # cut to multiple of cycles
    ncycles = np.floor(len(signal) / (samplerate/f))
    nsamples = int(ncycles * (samplerate/f))
    dat = signal[:nsamples]
    # remove mean
    dat = dat - np.mean(dat)
    # reference oscillator
    refc = np.cos(np.arange(len(dat)) * 2*np.pi * f/samplerate)
    refs = np.sin(np.arange(len(dat)) * 2*np.pi * f/samplerate)

    I = np.trapz(dat * refc) # noqa
    Q = np.trapz(dat * refs)
    A = np.sqrt(I**2 + Q**2) * 2 / len(dat)
    φ = np.arctan2(-Q, I)

    return ncycles, A, φ


def test_frequency(f, amp=1):
    rate, d1 = acquire_data(f, amp=amp, quiet=True)
    _, A1, φ1 = demodulate(d1, f, rate)
    I = (amp - A1) / RESISTANCE
    Z = A1 / I
    return rate, A1, φ1, I, Z


def two_point_impedance(fmin, fmax, steps, plot=True):
    if plot:
        plt.ion()
        fig, (ax1, ax2) = plt.subplots(nrows=2, sharex=True, figsize=(13, 8))
        ax1.axhline(1, color='lightgray', zorder=-1)
        ax1.set_xscale('log')
        ax1.set_yscale('log')
        ax1.set_xlim(fmin/1e3, fmax/1e3)
        ax2.set_xlabel('f / kHz')
        ax1.set_ylabel('signal amplitude / V')
        ax2.set_ylabel('impdance magnitude / Ohm')
        fig.canvas.draw()
        plt.tight_layout()
        plt.show(block=False)
        plt.pause(0.001)

    lines = []
    fs = np.logspace(np.log10(fmin), np.log10(fmax), steps)
    res = np.empty((steps, 7))
    for i, f in enumerate(fs):
        dat = (i, f) + test_frequency(f)
        print('{:5.1f}% {:.3e}Hz {:.2e}sps {:.3e}'.format(
            (i+1)/steps*100, f, dat[2], dat[3]))
        res[i] = dat
        if plot:
            for l in lines:
                l.remove()
            lines = []
            lines.append(ax1.plot(fs[:i+1]/1e3, res[:i+1, 3], color='C0')[0])
            lines.append(ax2.plot(fs[:i+1]/1e3, res[:i+1, 6], color='C2')[0])
            plt.tight_layout()
            plt.pause(0.001)
    return res


res = two_point_impedance(50e3, 200e3, 1000)
np.savetxt(str(np.datetime64('now'))+'.dat.txt', res)

while True:
    plt.pause(0.2)
