#!/bin/bash

# 配置参数
Protocol=$1
Target=$5
PORT=$2            # proftpd 服务器端口
FOLDER=$3    # seed 文件夹路径
SERVER="127.0.0.1" # proftpd 服务器地址 (本地或远程)
OUTFD=$4

# ARGS
echo $Target

# 检查文件夹是否存在
if [ ! -d "$FOLDER" ]; then
  echo "Error: Folder '$FOLDER' does not exist."
  exit 1
fi

# 另起一个后台进程来处理循环发送所有 .raw 文件
  
# 初始化计数器
i=1
mkdir -p "$OUTFD"
#/home/ubuntu/experiments/proftpd-gcov/proftpd -n -c /home/ubuntu/experiments/basic.conf -X

# 循环发送所有 .raw 文件
while [[ $i -le 13 ]]; do
  if [[ $i -eq 8 || $i -eq 9 ]]; then
    i=$((i+1))
    continue
  fi
  sleep 1
  seed_file="$FOLDER"/seed_$i.raw
  echo $seed_file
  if [ -f "$seed_file" ]; then
    # # 对第 i 个 seed_file 修改 --out 目录
    OUT_DIR="$OUTFD/$Protocol-$i"  # 使用 rtsp-i 作为目录名

    # # 运行命令并创建 rtsp-i 目录
    # mkdir -p "$OUT_DIR"

    # 开始运行 ./env-fuzz record 作为后台进程
    echo "Running: ./env-fuzz record  --out $OUT_DIR -- $Target"
    ./env-fuzz record  --out $OUT_DIR -- $Target &
    (
      sleep 1
      echo "Sending seed file: $seed_file to $SERVER:$PORT"

      # 使用 netcat 发送数据 (可以根据需要替换为 AFLNet replay)
      cat "$seed_file" | nc "$SERVER" "$PORT" -q 1

      echo "Message from $seed_file sent to $SERVER:$PORT"

      # # 获取所有进程并筛选出包含 env-fuzz 和 ld-linux-x86-64 的进程
      # pids=$(ps aux | grep -E "ld-linux-x86-64" | grep -v grep | awk '{print $2}')

      # # 如果找到了匹配的进程，则发送 SIGUSR1 信号
      # if [ -n "$pids" ]; then
      #   for pid in $pids; do
      #     echo "Sending SIGUSR1 to process $pid"
      #     kill -SIGUSR1 $pid
      #   done
      # else
      #   echo "No matching processes found."
      # fi
    )
    # wait
    echo "done if"
  fi
  # 增加计数器
  ((i=i+1))
done

exit
echo "All files sent."

# ./env-fuzz record -o pro-plus/ftp-8 -- /home/ubuntu/experiments/proftpd-gcov/proftpd -n -c /home/ubuntu/experiments/basic.conf -X
# cat experiments/in-ftp/seed_8.raw | nc 127.0.0.1 21 -q 1
# telnet 127.0.0.1 

# ./env-fuzz record -o pro-plus/ftp-9 -- /home/ubuntu/experiments/proftpd-gcov/proftpd -n -c /home/ubuntu/experiments/basic.conf -X
# cat experiments/in-ftp/seed_9.raw | nc 127.0.0.1 21 -q 1
# telnet 127.0.0.1