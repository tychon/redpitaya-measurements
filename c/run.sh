#/bin/bash
set -e

if [ "$#" -lt 2 ]; then
    echo "Arguments: REDPITAYA_IP EXECUTABLE_NAME [optional arguments]"
    exit 1
fi

RPIP="$1"
EXECNAME="$2"
# Now remove first two arguments from argument list,
# such that later "$@" are only the remaining arguments.
shift 2


# Red Pitaya has default passwort 'root' for user root.
# Since each Red Pitaya has its own host key, we ignore it.
# We also need to avoid loading /root/.profile, because it doesn't work over ssh.


# Load correct FPGA image for C API.
if [[ "$EXECNAME" == "init" ]]; then
    set -x
    sshpass -p 'root' ssh -q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RPIP" 'cat /opt/redpitaya/fpga/fpga_0.94.bit > /dev/xdevcfg'
    exit
fi

# Show commands
set -x

# Upload directory to Red Pitaya
sshpass -p 'root' scp -q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -r . "root@$RPIP:/root/measurements"

# Compile
sshpass -p 'root' ssh -q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RPIP" 'BASH_ENV=/etc/profile exec bash' <<EOF
  set -xe
  hostname
  cd measurements/
  make -B "$EXECNAME"
EOF

# Run and store output to stdout in compressed file
sshpass -p 'root' ssh -q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$RPIP" "LD_LIBRARY_PATH=/opt/redpitaya/lib measurements/$EXECNAME $@" | gzip -9 > ../output.gz
