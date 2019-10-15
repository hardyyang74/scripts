#!/system/bin/sh

echo "  "
echo "  "
echo "-------$#----------"

DSTPATH=/odm
tmppath="/data/.upgrade"

function clean_exit() {
  cd / && rm -rf $tmppath
  sync; sync; sleep 3
  killall uskin
}

function upos() {
  dis_update="$UPATH/start.bmp"
  dis_fail="$UPATH/fail.bmp"
  dis_ok="$UPATH/ok.bmp"

  #am force-stop com.percherry.roundadas
  bootapk=true
  killall "com.percherry.roundadas"
  source $DSTPATH/pos/bin/posStop.sh

  setprop vendor.percherry.uskin $dis_update

  rm -rf $tmppath
  mkdir $tmppath && cp $UPATH/uskin $tmppath/
  $tmppath/uskin &

  echo "cp upgrade package to $tmppath"
  if cp -rf $UPATH/* $tmppath/ ; then
    sync; sync
    echo "updating start from $tmppath"
  else
    echo "cp upgrade package fail!"
    setprop vendor.percherry.uskin $dis_fail
    clean_exit
  fi

  cd $tmppath
  apkname=`ls roundadas-*.apk`
  if [ x$apkname != "x" ]; then
    echo "install $apkname"
    pm install -r $apkname || echo "install $apkname fail !"
  else
    echo "not exist apk package"
  fi

  pkg=`ls pos-*-Android.tar.gz`
  if [ x$pkg != "x" ]; then
    echo "install pos!"
    posfolder=${pkg/%.tar.gz/}

    `tar xf $pkg`
    if [ $? -eq 0 ]; then
      echo "rename: $tmppath/$posfolder ---> $DSTPATH/pos"
#    mv $DSTPATH/pos $DSTPATH/pos.bk
      rm -rf $DSTPATH/pos
      mv $tmppath/$posfolder $DSTPATH/pos && chmod 777 $DSTPATH/pos
      # rm -rf $DSTPATH/pos.bk

      setprop vendor.percherry.uskin $dis_ok
    else
      echo "updage pos fail, resume pos!"
      setprop vendor.percherry.uskin $dis_fail
    fi
  fi

  echo "updating end"
  clean_exit
  source $DSTPATH/pos/bin/posStart.sh
}

NEW_UD=true
UPATH=`getprop vendor.percherry.upath`
bootapk=true

while [ true ]
do
  if [ "$bootapk" = true ]; then
    complete=`getprop sys.boot_completed`
    if [ x"$complete" = "x1" ]; then
      bootapk=false
      sleep 1
      am start -W -n com.percherry.roundadas/.RoundadasActivity
    fi
  fi

  if [ x"$UPATH" != "x" ]; then
    echo "UPATH: $UPATH"
    setprop vendor.percherry.upath ""
    upos $UPATH
  fi

  sleep 1
  if [ "$NEW_UD" = true ] && [ -d "/mnt/user/0/udisk/fwupdate" ]; then
    setprop vendor.percherry.upath "/mnt/user/0/udisk/fwupdate"
    NEW_UD=false
  elif [ "$NEW_UD" = false ] && [ ! -d "/mnt/user/0/udisk/fwupdate" ]; then
    NEW_UD=true
  fi
  UPATH=`getprop vendor.percherry.upath`
done
