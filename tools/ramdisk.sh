#!/bin/bash
# REQUIRES SUDO

#
# usage:  $ sudo ramdisk.sh [capacity=1024m] [mount_path=/ramdisk]
# mount a ramdisk device with given capacity and path
#

##
## How to Easily Create Ramdisk on Linux
## See: http://www.cppblog.com/wythern/archive/2020/02/26/217165.html
##

# Resolve $SCRIPT_SOURCE until the file is no longer a symlink
SCRIPT_SOURCE="${0}"
while [ -h "$PROGRAM_SOURCE" ]; do
    SCRIPT_DIR="$( cd -P "$( dirname "$SCRIPT_SOURCE" )" && pwd )"
    SCRIPT_SOURCE="$(readlink "$SCRIPT_SOURCE")"
    # If $SCRIPT_SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
    [[ $SCRIPT_SOURCE != /*  ]] && SCRIPT_SOURCE="$DIR/$SCRIPT_SOURCE"
done
SCRIPT_DIR="$( cd -P "$( dirname "$SCRIPT_SOURCE" )" && pwd )"

SCRIPT_NAME=$(basename $0)
echo "Starting ${SCRIPT_DIR}/${SCRIPT_NAME} ..."

# become root if not already
if [ $EUID != 0 ]; then
    sudo "${0}"
    exit $?
fi

# default capacity=1024(MiB)
capacity="1024m"
if [ -n "${1}" ]; then
    capacity="${1}"
fi

# default mount_path="/ramdisk"
mount_path="/ramdisk"
if [ -n "${2}" ]; then
    mount_path="${2}"
fi

sudo mkdir -p "${mount_path}"
sudo chmod 777 "${mount_path}"

set -x
sudo mount -t tmpfs -o size="${capacity}" ramdisk "${mount_path}"
set +x

sudo mount | tail -n 1

if [ -d "${SCRIPT_DIR}../data" ]; then
    cp "${SCRIPT_DIR}../data/*.txt" "${mount_path}"
    echo "All text files of [${SCRIPT_DIR}../data] folder have been copied to [${mount_path}]."
else
    echo "[${SCRIPT_DIR}../data] folder is not exists."
fi
