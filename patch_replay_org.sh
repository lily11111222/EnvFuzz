#!/bin/bash
# outfd=$1
# covfile=$2

# 初始化计数器

envdir="../EnvFuzz"
expdir="../experiments"


cd $envdir
j=1
while [ $j -le 13 ] ; do
  ./replay_and_log.sh "proftpd-out/ftp-$j/log.log" 0 "-o proftpd-out/ftp-$j"
j=$((j + 1))
done