
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


# Run Python SCPI scripts

In `python-scpi/` directory run

    python measure-two-point.py IPADDR
