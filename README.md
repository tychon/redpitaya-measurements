
# Run C programs

C code needs to be compiled with headers an libraries on Red Pitaya
OS.  Use the `c/run.sh` script to copy everything to the Red Pitaya
via ssh, compile there and execute.

Before running code use

    bash run.sh IPADDR init

to load FPGA image for C API.  Then you can run for example

    bash run.sh IPADDR avoided_coupling_2channels.x

The output to `stdout` will be downloaded to `output.txt`.

[Following the documentation.](https://redpitaya.readthedocs.io/en/latest/developerGuide/comC.html)

## Chain of Red Pitayas
By connecting the EXT_TRIG pins of multiple Red Pitayas to one the
trigger signal of one leading Red Pitaya you can increase the number
of channels available for signal capture, driving and delayed
triggers.

This is automated in `run-chain.sh` which uploads, compiles and runs
the code on all Red Pitayas whose IPs are supplied.  For example

    bash run-chain.sh IPADDR1 IPADDR2 oscilloscope_gpio.x 0,10,10e-6

**Note:** The code is only uploaded and recompiled if it changed since
the last execution.  The last execution is set as the modification
time of the `run-chain.sh` file.  To force recompiling, e.g. because
the chain ordering or make target changed, `touch` any other source
file.

You may want to force recompilation after changing the order of
devices in the chain, because already during compile time the leader
(first device) and followers are set.

Just as `run.sh` the initialization (`init`; and also `ping`) is supported:

    bash run-chain.sh IPADDR1 IPADDR2 init

Outputs are concatenated into `output.gz` to have all datasets as one.
Red Pitaya input channels are 1 and 2 for first RP in list of IPs, 3
and 4 for next and so on.

# Live Explorer
The `pyqtgraph` python package is required.  First upload and compile
the RP script.  In the `c/` folder run

    bash run.sh IPADDR init
    bash run.sh IPADDR live-explorer.x

You can interrupt the script after successful compilation.  Then in
`live-explorer/` run

    ssh root@IPADDR 'LD_LIBRARY_PATH=/opt/redpitaya/lib measurements/live-explorer.x' | python fftviewer.py 86e3 66e3

or to avoid having to enter the root password (which is `root`) use
`sshpass`:

    sshpass -p root ssh -q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@169.254.72.1 'LD_LIBRARY_PATH=/opt/redpitaya/lib measurements/live-explorer.x' | python fftviewer.py 86e3 66e3

# Run Python SCPI scripts
In `python-scpi/` directory run

    python measure-two-point.py IPADDR
