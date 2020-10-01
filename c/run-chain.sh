#/bin/bash
# Run measurement script on Red Pitaya.
#
# Scripts are uploaded to Red Pitaya and compiled there, because there
# the headers and libraries are available to gcc.
# The same code is uploaded to all devices and upon compiling all
# execept for the first devices are compiled with -DFOLLOW
#
# Note: Since upload and compilation takes considerable time, it is
# done only when any file in the directory has newer modification time
# than this script file. This script file is `touch`ed for on every
# execution. `init` command touches Makefile to trigger new upload.
#
# Red Pitaya has default passwort 'root' for user root.
# Since each Red Pitaya has its own host key, we ignore it.
# We also need to avoid loading /root/.profile, because it doesn't work over ssh.


# Exit on failure of any command
set -e

if [ "$#" -lt 2 ]; then
    echo "Arguments: IP1 [IP2...] EXECUTABLE [optional arguments]"
    echo "Additional commands instead of EXECUTABLE: ping, init"
    exit 1
fi

# Parse IPs from arguments
IPs=""
for arg in "$@"; do
    if [[ $arg =~ ^[0-9.]+$ ]]; then
        IPs+=" ${arg}"
    else
        break
    fi
done

IPs=( $IPs )
N=${#IPs[@]}
IPs="${IPs[*]}"
echo "Chain of $N devices."

# Now remove IPs from argument list,
# such that later "$@" are only the remaining arguments.
shift $N
EXECNAME="$1"
echo $EXECNAME
shift 1

### Additional commands that are not executables
# Ping addresses
if [[ "$EXECNAME" == "ping" ]]; then
    for RPIP in $IPs; do
        ping -c 3 -i 0.2 -a $RPIP
    done
    exit
fi

# Load correct FPGA image for C API.
if [[ "$EXECNAME" == "init" ]]; then
    set -x
    for RPIP in $IPs; do
        sshpass -p 'root' ssh -q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
                "root@$RPIP" 'cat /opt/redpitaya/fpga/fpga_0.94.bit > /dev/xdevcfg'
    done
    touch Makefile
    exit
fi


### Upload executables and recompile
LASTMOD=$(find . -type f -not -name run-chain.sh -not -name '#*' -printf '%T@ %p\n' \
              | sort -n | tail -1 | cut -f1 -d".")
LASTRUN=$(date -r $(basename "$0") +%s)
touch `basename "$0"`
CHAINFLAG=""
if [ $LASTMOD -gt $LASTRUN ];
then
    for RPIP in $IPs; do
        set -x
        # Upload directory
        sshpass -p 'root' scp -q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
                -r . "root@$RPIP:/root/measurements"

        # Compile
        sshpass -p 'root' ssh -q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
                "root@$RPIP" 'BASH_ENV=/etc/profile exec bash' <<EOF
set -xe
hostname
cd measurements/
make CHAINFLAG=$CHAINFLAG -B "$EXECNAME"
EOF
        { set +x; } 2> /dev/null # silently disable xtrace
        CHAINFLAG="-DFOLLOW"
    done
fi

# Start programm in reverse order such that triggering device is started last.
IDX=$N # Count indices decreasing for output file names.
for RPIP in $(echo $IPs | tac -s " "); do # Loop in reverse order
    # Run and store output to stdout in compressed file
    set -x
    sshpass -p 'root' ssh -q -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
            "root@$RPIP" "LD_LIBRARY_PATH=/opt/redpitaya/lib measurements/$EXECNAME $@ $((IDX*2-2))" \
        | gzip -9 > ../output_$IDX.gz && echo "fin $RPIP" &
    { set +x; } 2> /dev/null # silently disable xtrace
    IDX=$((IDX-1))
    SLEEP=""
done

set -x
# Kill background processes on Ctrl-C
trap 'kill $(jobs -p)' INT
# Wait for background processes
wait

# Concatenate outputs from RPs into one file.
shopt -s extglob
zcat ../output_+([0-9]).gz | gzip -9 > ../output.gz
