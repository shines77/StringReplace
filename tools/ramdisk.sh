#!/bin/bash
# REQUIRES SUDO

#
# usage:  $ sudo ramdisk.sh [capacity=1024] [mount_path=/ramdisk] [label=ramdisk]
#                                   =(1024 MB)
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
    # capacity unit: (MiB)
    capacity="${1}m"
fi

# default mount_path="/ramdisk"
mount_path="/ramdisk"
if [ -n "${2}" ]; then
    mount_path="${2}"
fi

label="ramdisk"
if [ -n "${3}" ]; then
    label="${3}"
fi

##
## "$?" is the grep result
## grep 'match_word' file ; echo $?
##

HasMounted=`df -h | grep "${mount_path}"`
# echo "HasMounted = ${HasMounted}"

if [ -z "${HasMounted}" ]; then
    if [ ! -d "${mount_path}" ]; then
        sudo mkdir -p "${mount_path}"
        sudo chmod 777 "${mount_path}"
    fi

    ##
    ## [ramfs] (This filesystem type can't set the capacity size)
    ##
    ##   See: https://developer.aliyun.com/article/746214
    ##
    ##   mount -t ramfs ramfs /mnt/ramdisk -o noatime,nodiratime,rw,data=writeback,nodelalloc,nobarrier
    ##

    set -x
    sudo mount -t tmpfs -o size="${capacity}" "${label}" "${mount_path}"
    set +x

    echo ""
    sudo mount | tail -n 3
    echo ""
else
    echo ""
    sudo mount | tail -n 3
    echo ""
    echo "--------------------------------------------------------------------"
    echo " Label: [ ${label} ], Mount-point: [ ${mount_path} ] have mounted."
    echo "--------------------------------------------------------------------"
    echo ""
fi

DATA_DIR=$(cd "${SCRIPT_DIR}"; cd ../data; pwd)
if [ -d "${DATA_DIR}" ]; then
    cp -t "${mount_path}" "${DATA_DIR}"/*.txt
    echo "All text files under [ ${DATA_DIR} ] have been copied to [ ${mount_path} ]."
else
    echo "Folder [ ${DATA_DIR} ] is not exists."
fi
