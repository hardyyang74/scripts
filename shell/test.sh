#!/bin/bash

while getopts "abc" name; do
  echo $OPTIND

  case $name in
    a)
      echo "para a"
      ;;

    b)
      echo "para b"
      ;;
    *) echo $name
      ;;
  esac
done

echo $*
shift $(($OPTIND - 1))
echo "$OPTIND: $*"
echo "$1"

exit 0

testreturn() {
  echo "test 1"
  echo "test 2"
  return 3
}

echo "test 3"
num=`testreturn`
echo $?
echo "$num"

for i in `seq 3`; do
  echo $i
done

count_no() {
  read num < ./bootnum || num=3
  num=$((num=num-1))

  echo $num > ./bootnum
}
