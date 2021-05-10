"""
Run the same program on many RPs and read their outputs (line buffered).
Assumes the output format

    CH SAMPLES...

With RBBUFFERSIZE samples.  Also implements a ring buffer to keep all
these samples.
"""

import subprocess
import select
import numpy as np


RPBUFFERSIZE = 16384  # = 2**14
SSHCMD = "sshpass -p root ssh -q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@{IP} 'LD_LIBRARY_PATH=/opt/redpitaya/lib measurements/live-explorer.x'"


class RPChain:
    def __init__(self):
        # list of [(ip, subprocess, csv-reader)]
        self.connections = []

    def channelnum(self):
        return 2 * len(self.connections)

    def connect(self, ips):
        for ip in ips:
            proc = subprocess.Popen(
                SSHCMD.format(IP=ip), shell=True,
                encoding='ascii', universal_newlines=True,
                bufsize=1,  # line buffered
                stdout=subprocess.PIPE)
            self.connections.append((ip, proc))

    def read(self, timeout=0):
        records = []  # list of (ch, samples)
        fds = [proc.stdout for ip, proc in self.connections]
        rlist, _, _ = select.select(fds, [], [], timeout)
        while rlist:
            for r in rlist:
                idx = fds.index(r)
                ip, proc = self.connections[idx]
                values = [float(x) for x in r.readline().split()]
                if len(values) != 1+16384:
                    print(f"{ip} invalid line ({len(values)} values)")
                else:
                    ch = 2*idx + int(values[0])-1
                    print(f"{ip}-{int(values[0])} {ch} valid")
                    records.append((ch, values[1:]))
            rlist, _, _ = select.select(fds, [], [], 0)
        return records


class RingBufferChain:
    def __init__(self, rpchain, nring, fill=0,
                 transform=None, nvalues=RPBUFFERSIZE, dtype=float):
        self.rpchain = rpchain
        self.transform = transform
        self.buffer = np.full(
            (rpchain.channelnum(), nring, RPBUFFERSIZE),
            fill, dtype=float)

        if transform is not None:
            self.transformed = np.full(
                (rpchain.channelnum(), nring, nvalues),
                fill, dtype=dtype)

    def read(self, timeout=0):
        records = self.rpchain.read(timeout)
        for idx, values in records:
            ring = self.buffer[idx]
            ring = np.roll(ring, 1, axis=0)
            ring[0, :] = values
            self.buffer[idx] = ring

            if self.transform is not None:
                tring = self.transformed[idx]
                tring = np.roll(tring, 1, axis=0)
                tring[0, :] = self.transform(values)
                self.transformed[idx] = tring
        return set(idx for idx, values in records)


# Test with RPs listed as arguments for 1 second.
if __name__ == '__main__':
    import time
    import sys
    chain = RPChain()
    chain.connect(sys.argv[1:])
    time.sleep(1)
    chain.read()
