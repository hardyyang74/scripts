#!/bin/sh

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
