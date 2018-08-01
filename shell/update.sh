#!/bin/sh

BOOTEMMC="`grep /dev/mmcblk0p -c /proc/cmdline`"
BOOTRECOVERY="`grep /dev/mmcblk0p3 -c /proc/cmdline`"
KERNELV=$(awk {'print $3'} /proc/version)

image_path=$(dirname $0)
target=''
echo 'update system from '$image_path

paths=("gst-app" "/home"
"gst-plugin" "/usr/lib/gstreamer-0.10"
"gst-lib" "/usr/lib"
"GIFT" "/GIFT"
"config" "/config"
"usr" "/usr"
"etc" "/etc"
)

upend() {
  end_word=$1

  echo -e "\t\033[31m$end_word\033[0m\n\a\a\a" > /dev/tty0
}

repair_emmc() {
  mount -o remount,ro /
  fsck.ext4 /dev/mmcblk0p1 -y
  fsck.ext2 /dev/mmcblk0p3 -y
}

writeUbootKernl() {
  if  [ -e "$image_path/u-boot.bin" ]; then
    echo "update uboot"
    echo -ne "\tupdate uboot..." > /dev/tty0
    # uboot env section, refer to CONFIG_ENV_OFFSET of uboot
    dd if=/dev/zero of=/dev/mmcblk0 bs=512 seek=1536 count=16

    echo 0 > /sys/block/mmcblk0boot0/force_ro
    dd if=$image_path/u-boot.bin of=/dev/mmcblk0boot0 bs=512 seek=2 skip=2 || { repair_emmc; exit -1; }
    echo 1 > /sys/block/mmcblk0boot0/force_ro

    echo 0 > /sys/block/mmcblk0boot1/force_ro
    dd if=$image_path/u-boot.bin of=/dev/mmcblk0boot1 bs=512 seek=2 skip=2
    echo 1 > /sys/block/mmcblk0boot1/force_ro
    echo "done" > /dev/tty0
  fi

  # refer to CONFIG_ENV__CUS_BOOTVIDEO_OFFSET of uboot
  if [ x$BOOTRECOVERY != x"1" ] && [ -e "$image_path/cusvideo.txt" ]; then
    while read line;do
      if [ ${#line} -gt 16 ]; then
        eval "$line"
      fi
    done < "$image_path/cusvideo.txt"
    if [ "x$video" = "x" ]; then
      video='mxcfb0:dev=pt1000,BT656-PAL,if=BT656,fbpix=UYVY16'
    fi
    echo "video=$video"

    dd if=/dev/zero of=/dev/mmcblk0 bs=512 seek=1540 count=1
    echo -n "video=$video" | tr -d "\r\n" | dd of=/dev/mmcblk0 bs=512 seek=1540 count=1
  fi
  
  if  [ -e "$image_path/uImage" ]; then
    echo "update kernel"
    echo -ne "\tupdate kernel..." > /dev/tty0
    dd if=$image_path/uImage of=/dev/mmcblk0 bs=256k seek=4 conv=fsync || { repair_emmc; exit -2; }
    echo 22 > /sys/devices/platform/sdhci-esdhc-imx.3/mmc_host/mmc0/mmc0:0001/boot_bus_config
    echo 8 > /sys/devices/platform/sdhci-esdhc-imx.3/mmc_host/mmc0/mmc0:0001/boot_config
    echo "done" > /dev/tty0
  fi
}

upgradeApp(){
  # write file system
  if [ -f $image_path/upgrade.bin ]; then
    echo 'upgrade file system...'
    echo -ne "\tupgrade file system... " > /dev/tty0
    tar zxf $image_path/upgrade.bin -C /
    sync && echo "done" > /dev/tty0

    [ -f /rml.txt ] && { echo "rm unuse files"; cat /rml.txt | xargs rm -rf; rm -f /rml.txt; }
  fi
}

customize() {
  echo "customize..."
  echo -ne "\tcustomize." > /dev/tty0

  for (( i = 0; i < ${#paths[*]}; i += 2 ))
  do
    if [ -d $image_path/${paths[i]} ]; then
      mkdir -p $target${paths[i+1]}

      echo $image_path/${paths[i]}'/*  ===>>> '$target${paths[i+1]}/
      [ "`ls -A $image_path/${paths[i]}`" != "" ] && { cp -rf $image_path/${paths[i]}/* $target${paths[i+1]}/ || { repair_emmc; exit -4; };}
      echo -n "." > /dev/tty0
    fi
  done

  echo "done"  > /dev/tty0
}

write_fs() {
  echo -ne "\tburn $fname ... 0%" > /dev/tty0
  fssize=$((`ls -l $1 | awk {'print $5'}`))
  copied=0
  fname=${1##*/}

  rm -f /tmp/tmp.txt 2>/dev/null
  dd if=$1 of=$2 bs=256k 2>/tmp/tmp.txt &
  while [ x"`ps|grep $2|grep -v grep`" != "x" ]; do
    sleep 1
    killall -USR1 dd
    copied=$((`tail -n 1 /tmp/tmp.txt | awk {'print $1'}`))
    percent=$((copied*100/fssize))
    echo -ne "\r\tburn $fname ... $percent%" > /dev/tty0
#    echo "$copied $fssize $percent"
  done
  echo -e "\r\tburn $fname ... 100%" > /dev/tty0
}

writer_recovery(){
    res=`fsck -N /dev/mmcblk0p3 | grep "ext2" -c`
    if [ $res -eq 0 ];then
      echo "change recovery partition to ext2 filesystem"
      mkfs.ext2 -j /dev/mmcblk0p3
      tune2fs -c -1 -i 0 -o journal_data_writeback /dev/mmcblk0p3
    fi

    write_fs "$image_path/recovery.img" "/dev/mmcblk0p3"
}

writeFW(){
  # write file system
  if [ -f $image_path/anpark.img ]; then

    if [ ! -e /dev/mmcblk0p1 ];then
      # format emmc
      echo -e "\tpartition emmc" > /dev/tty0
      
      caps=$((`fdisk -l /dev/mmcblk0 | grep "Disk /dev/mmcblk0:" | awk {'print $5'}`))
      units=$((`fdisk -l /dev/mmcblk0 | grep "Units" | awk {'print $9'}`))
      P1BEG=$((8*1024*1024/units+1))
      P2BEG=$((256*1024*1024/units + P1BEG))
      P3BEG=$(((caps - 64*1024*1024)/units))
      P1END=$((P2BEG - 1))
      P2END=$((P3BEG - 1))
      #echo "($P1BEG $P1END) ($P2BEG $P2END) ($P3BEG n)"
      cmd=("n" "p" "1" "$P1BEG" "$P1END"
        "n" "p" "2" "$P2BEG" "$P2END"
        "n" "p" "3" "$P3BEG" "\n"
        "p" "w")
      
      for str in ${cmd[*]}
      do
        #yes $str | head -1
        echo -e $str
        #  sleep 1
      done | fdisk /dev/mmcblk0

      echo -e "\tformat emmc" > /dev/tty0
      mkfs.ext4 -j /dev/mmcblk0p1
      mkfs.ext4 -b 4096 -i 409600 /dev/mmcblk0p2
      mkfs.ext2 -j /dev/mmcblk0p3

      tune2fs -c -1 -i 0 -o journal_data_writeback /dev/mmcblk0p1
      tune2fs -c -1 -i 0 -o journal_data_writeback /dev/mmcblk0p2
      tune2fs -c -1 -i 0 -o journal_data_writeback /dev/mmcblk0p3
    fi
  
    if [ -f $image_path/anpark.img ]; then
      write_fs "$image_path/anpark.img" "/dev/mmcblk0p1"
    fi

    writer_recovery
  fi
}

if [ ! -d /lib/modules/$KERNELV ]; then
  ln -s /lib/modules/3.0.35-2666-gbdde708 /lib/modules/$KERNELV
fi
modprobe fbcon
echo -e "\n\n" > /dev/tty0

if [ -f /home/virtual_key.ko ] && [ -f /home/service ] ; then
  insmod /home/virtual_key.ko
  /home/service &
fi

echo "BOOTEMMC:$BOOTEMMC BOOTRECOVERY:$BOOTRECOVERY"

if [ $BOOTEMMC == '1' ]; then
  # boot from emmc, upgrade
  if [ x$BOOTRECOVERY == x"1" ]; then
    writeUbootKernl
    if [ -f $image_path/anpark.img ]; then
      write_fs "$image_path/anpark.img" "/dev/mmcblk0p1"
    fi

#    customize

    # resume recovery flag
    echo "reset recovery flag"
    dd if=/dev/zero of=/dev/mmcblk0 bs=1 seek=786432 count=1

    sync && sleep 1
    upend "recovery done, please remove usb storage and reboot machine!"
    echo "recovery end."
    umount -a -r -n
    halt -f
  fi
  if [ -f $image_path/recovery.img ];then
    echo -e "\tRecovery will start,keep the power on during the process!!!" > /dev/tty0
    writer_recovery
    writeUbootKernl

    # set recovery flag
    # reference to burn uboot
    # dd if=/dev/zero of=/dev/mmcblk0 bs=512 seek=1536 count=16
    yes "R" | dd of=/dev/mmcblk0 bs=1 seek=786432 count=1

    sync && sync
    upend "Now,update program will restart your machine"

    echo -ne "\tPlease wait 3" > /dev/tty0
    sleep 1
    echo -ne "\r\tPlease wait 2" > /dev/tty0
    sleep 1
    echo -ne "\r\tPlease wait 1" > /dev/tty0
    sleep 1
    reboot -f
  else
    upgradeApp
    writeUbootKernl
    [ -f /etc/dev.tar ] && rm -f /etc/dev.tar
    customize
  fi
else
  image_path=$image_path/fw/
  writeFW
  writeUbootKernl
fi

sync && sleep 1
