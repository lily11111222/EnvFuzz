#!/bin/bash

log_file=$1  # log文件
patch=$2   # 要replay的patch
option=$3

echo $patch
echo $option
if [[ $patch -eq 0 ]]; then
    echo "patch=0"
    ./env-fuzz replay --log=3 $option > $log_file 2>&1
else
    ./env-fuzz replay --log=3 $patch $option > $log_file 2>&1
fi

sed -i "s/\x1b\[31m//g" $log_file
sed -i "s/\x1b\[32m//g" $log_file
sed -i "s/\x1b\[33m//g" $log_file
sed -i "s/\x1b\[34m//g" $log_file
sed -i "s/\x1b\[35m//g" $log_file
sed -i "s/\x1b\[36m//g" $log_file
sed -i "s/\x1b\[0m//g" $log_file