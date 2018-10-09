#!/bin/bash

udisk=''
uboot=$PWD/u-boot.bin
uimage=$PWD/uImage
fs=$PWD/anpark.img

format_udisk=false
chang_panel=false

showhelp() {
  echo "usage:"
  echo "  first arg -- udisk path"
  echo "  other args -- "
  echo '    -b boot image, default: "u-boot.bin" in current path'
  echo '    -k kernel image, default: "uImage" in current path'
  echo '    -f file system tar package, default: "anpark.img" in current path'
  echo '    -F format udisk'
  echo '    -c u-boot.bin-pre3-656 or u-boot.bin-pre3-ahd or u-boot.bin-pre3-888'
  echo '       change display panel'
  echo '       -b -k -f will be invalid'
}

getarg() {
  while getopts b:k:f:c:F name
  do
    case $name in
      b) uboot=$OPTARG ;;
      k) uimage=$OPTARG ;;
      f) fs=$OPTARG ;;
      F) format_udisk=true ;;
      c)
         chang_panel=true
         format_udisk=false
         uboot=$OPTARG
         ;;
      *) invalid para ;;
    esac
  done
}

partition() {
  # umount udisk
  ls $udisk* | grep -vw $udisk | xargs sudo umount

  if $format_udisk ; then
    echo 'format udisk...'
    cmd=("y" "\n" "\n")
    for str in ${cmd[*]}
    do
#      yes $str | head -1
      echo -e $str
    done | mkfs.ext4 $udisk

    cmd=("n" "p" "1" "16384" "\n"
      "p" "w")

    for str in ${cmd[*]}
    do
#     yes $str | head -1
      echo -e $str
    done | fdisk $udisk

    cmd=("\n" "\n")
    for str in ${cmd[*]}
    do
      echo -e $str
    done | mkfs.ext4 -j $udisk'1'

    tune2fs -c -1 -i 0 -o journal_data_writeback $udisk'1'
  fi
}

if [ $# -lt 1 ] ; then
   showhelp
   exit 1
else
  udisk=$1
  shift

  getarg $*
fi

partition

if [ -f $uboot ]; then
  dd if=$uboot of=$udisk bs=512 seek=2 skip=2
fi
if ! $chang_panel && [ -f $uimage ]; then
  dd if=$uimage of=$udisk bs=1M seek=1
fi

if [ -f $fs ]; then
  mkdir -p /mnt/bootsd && mount $udisk'1' /mnt/bootsd/

  if ! $chang_panel ; then
    rm -rf /mnt/bootsd/fw && mkdir -p /mnt/bootsd/fw
    mount $fs /mnt/bootsd/fw && cp -rf /mnt/bootsd/fw/* /mnt/bootsd/
    umount /mnt/bootsd/fw
    ln -sf fw/update.sh /mnt/bootsd/updateFW.sh

    cp update.sh $uimage $fs recovery.img cusvideo.txt /mnt/bootsd/fw/
  fi
  cp $uboot /mnt/bootsd/fw/u-boot.bin
  sync
  sleep 3

  umount $udisk'1'
  rmdir /mnt/bootsd
fi
