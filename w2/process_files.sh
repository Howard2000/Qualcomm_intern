#!/bin/bash

target=$1
output_file="result.log"

> "$output_file" #创建文件

find . -type f -name "*.txt" | grep -F "$target" > "$output_file"

# 统计匹配的文件数量
count=$(wc -l < "$output_file")

# 修改匹配文件的权限为只读（444）
while IFS= read -r file; do #读取整行，忽略/， 分隔符
    chmod 444 "$file"
done < "$output_file"

# 输出完成信息
echo "查找到 $count 个文件，权限已更改为只读。操作完成！"
