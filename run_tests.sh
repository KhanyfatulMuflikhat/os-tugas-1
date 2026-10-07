#!/bin/bash
echo "Compiling scheduler.c..."
gcc Scheduler.c -o scheduler

mkdir -p output

for f in testcases/*.txt; do
    name=$(basename "$f" .txt)
    echo "Menjalankan testcase: $name"
    ./scheduler < "$f" > "output/$name.txt"
done

echo "Semua testcase selesai, hasil tersimpan di folder output/"
