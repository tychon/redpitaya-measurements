#/bin/bash
set -x

if [ "$#" -ne 2 ]; then
    echo "Arguments: IP of redpitaya and executable name"
    exit 1
fi


# Red Pitaya has default passwort 'root' for user root.
# Since each Red Pitaya has its own host key, we ignore it.
# We also need to avoid loading /root/.profile, because it doesn't work over ssh.


# Load correct FPGA image for C API.
if [[ "$2" == "init" ]]; then
    sshpass -p 'root' ssh -q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$1" 'cat /opt/redpitaya/fpga/fpga_0.94.bit > /dev/xdevcfg'
    exit
fi

# Upload directory to Red Pitaya
sshpass -p 'root' scp -q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -r . "root@$1:/root/measurements"

# Compile and run
sshpass -p 'root' ssh -q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$1" 'BASH_ENV=/etc/profile exec bash' <<EOF
  set -x
  hostname
  cd measurements/
  make -B "$2" || exit
  LD_LIBRARY_PATH=/opt/redpitaya/lib "./$2" > output.txt
EOF

# Download output data
sshpass -p 'root' scp -q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "root@$1:/root/measurements/output.txt" .
