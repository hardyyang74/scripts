#!/bin/sh

for i in `seq 60`; do
  echo $i
done

count_no() {
  read num < ./bootnum || num=3
  num=$((num=num-1))

  echo $num > ./bootnum
}

compare_num() {
  curVer=`expr $1 % 10`
  srcVer=`expr $2 % 10`

  curCus=`expr $1 / 10`
  srcCus=`expr $2 / 10`

  echo "curCus:$curCus srcCus:$srcCus curVer:$curVer srcVer:$srcVer"

  if [ $curVer != $srcVer ] || [ $curCus != 0 ] && [ $curCus != $srcCus ]; then
    echo "updating refuse"
  else
    echo "updating start"
  fi
}

echo "curVer:$1 srcVer:$2"
compare_num $1 $2

