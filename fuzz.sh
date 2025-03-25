#!/bin/bash

# 配置参数
FOLDER=$1    # Envfuzz corpus 文件夹路径
Protocol=$2
TOL=$3      # in-seed的数量
FUZ_TIME=$4 # fuzz的时间，单位是h

i=1
if [ -z "$TOL" ]; then
    echo "Error: Tol is empty or undefined"
    exit 1
fi
# fuzz_time=$(echo "scale=0; (24 / $Tol) * 3600" | bc)
fuzz_time=$(echo "scale=0; ($FUZ_TIME * 3600) / $TOL" | bc)
echo $fuzz_time
# 循环发送所有 .raw 文件
while [ $i -le $TOL ] ; do
  echo "doing ./env-fuzz fuzz --out $FOLDER/$Protocol-$i --max-time $fuzz_time"
  sudo ./env-fuzz fuzz --out $FOLDER/$Protocol-$i --max-time $fuzz_time
  # 增加计数器
  ((i=i+1))
done

echo "For $Protocol, all files fuzzed."


# # 获取所有进程并筛选出包含 env-fuzz 和 ld-linux-x86-64 的进程
# pids=$(ps aux | grep -E "env-fuzz|ld-linux-x86-64" | grep -v grep | awk '{print $2}')

# # 如果找到了匹配的进程，则发送 SIGUSR1 信号
# if [ -n "$pids" ]; then
#   for pid in $pids; do
#     echo "Sending SIGUSR1 to process $pid"
#     kill -SIGUSR1 $pid
#   done
# else
#   echo "No matching processes found."
# fi
