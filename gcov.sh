#!/bin/bash
outfd=$1
covfile=$2

# 初始化计数器
i=1
proftpd1="proftpd1"
proftpd2="proftpd2"

envdir="../EnvFuzz"
expdir="../experiments"

cd $envdir
# 删除已有的 coverage 文件
# # rm -f ../experiments/$covfile; 
# touch ../experiments/$covfile
# echo "Time,l_per,l_abs,b_per,b_abs" >> ../experiments/$covfile

# 删除 proftpd1 并清理 proftpd2 中的 .gcda 文件
# rm -rf "$expdir/$proftpd1"
# if [ -d "$expdir/$proftpd2" ]; then
#   find "$expdir/$proftpd2" -name "*.gcda" -delete
# fi

# 遍历 out 文件夹下的每个子文件夹
for mdir in $outfd/queue/*; do
  cd $envdir
  # echo $mdir
  # 检查当前目录是否是文件夹
  if [ -d "$mdir" ]; then
    # 遍历子文件夹中的所有 .patch 文件
    for patch_file in "$mdir"/*.patch; do
      cd $envdir
      time=$(stat -c %Y $patch_file)
      # 执行 './env-fuzz replay' 命令
      echo "Executing: ./env-fuzz replay -o $outfd $patch_file"
      timeout 2s ./env-fuzz replay -o $outfd "$patch_file" > /dev/null 2>&1
      exit_status=$?
      # 检查是否因为超时退出
      if [[ $exit_status -eq 124 ]]; then
        echo "Timeout reached for $patch_file, skipping to next."
        continue
      fi

      cd $expdir
      
      if [[ i -eq 1 && $outfd = "proftpd-out/ftp-1" ]]; then
        cp -r proftpd-gcov "proftpd1"
        cp -r proftpd-gcov "proftpd2"
      #   cov_data=$(gcovr -r "$proftpd1" -s | grep "[lb][a-z]*:")
      # else
      #   echo "Copying proftpd to proftpd_copy_$i"
      #   cp -r proftpd "proftpd_copy_$i"
      fi
      # 复制 proftpd 文件夹到 proftpd_copy_i
      echo "Copying proftpd to proftpd_copy_$i"
      cp -r proftpd-gcov "proftpd_copy_$i"

      # 第一轮不进行 gcov-tool merge
      # if (( i > 1 )); then
      if (( i % 2 == 1 )); then
        echo "Executing: gcov-tool merge proftpd_copy_$i $proftpd1 -o $proftpd2"
        gcov-tool merge "proftpd_copy_$i" "$proftpd1" -o "$proftpd2"
        cov_data=$(gcovr -r "$proftpd2" -s | grep "[lb][a-z]*:")
      else
        echo "Executing: gcov-tool merge proftpd_copy_$i $proftpd2 -o $proftpd1"
        gcov-tool merge "proftpd_copy_$i" "$proftpd2" -o "$proftpd1"
        cov_data=$(gcovr -r "$proftpd1" -s | grep "[lb][a-z]*:")
      fi
      # fi

      l_per=$(echo "$cov_data" | grep lines | cut -d" " -f2 | rev | cut -c2- | rev)
      l_abs=$(echo "$cov_data" | grep lines | cut -d" " -f3 | cut -c2-)
      b_per=$(echo "$cov_data" | grep branch | cut -d" " -f2 | rev | cut -c2- | rev)
      b_abs=$(echo "$cov_data" | grep branch | cut -d" " -f3 | cut -c2-)
      
      echo "$time,$l_per,$l_abs,$b_per,$b_abs"
      echo $covfile
      echo "$time,$l_per,$l_abs,$b_per,$b_abs,$patch_file" >> ../experiments/$covfile

      # 删除 proftpd_copy_$i 文件夹
      echo "Deleting proftpd_copy_$i"
      rm -rf "proftpd_copy_$i"

      # 更新计数器
      i=$((i + 1))
    done
  fi
done
