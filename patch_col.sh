#!/bin/bash
covfile=$1
TOL=$2

proout_dir="ptoftpd-out"
rm -f ../experiments/$covfile; 
touch ../experiments/$covfile
echo "Time,l_per,l_abs,b_per,b_abs" >> ../experiments/$covfile

i=1
# 遍历 proout 文件夹下的所有子文件夹
# for dir in "$proout_dir"/*; do
while [ $i -le $TOL ] ; do
  echo "doing $proout_dir/proftpd-$i"
  # 检查是否是文件夹
  if [ -d "$proout_dir/proftpd-$i" ]; then
    echo "in doing $proout_dir/proftpd-$i"
    echo "Executing: ./gcov.sh $proout_dir/proftpd-$i result.csv"
    ./gcov.sh "$proout_dir/proftpd-$i" "$covfile"
  fi
  # 增加计数器
  ((i=i+1))
done