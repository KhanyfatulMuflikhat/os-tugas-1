#!/bin/bash

echo "==================================="
echo "TUGAS 1 OS - KELOMPOK C4"
echo "==================================="
echo "Mengecek sistem..."

# 1. OS/Kernel
os_info=$(uname -sr)
echo "OS/Kernel        : $os_info"

# 2. Jumlah user biasa
jumlah_user=$(awk -F: '$3 >= 1000 && $3 < 60000 {print $1}' /etc/passwd | wc -l)
echo "Akun pengguna    : $jumlah_user akun"

# 3. Jumlah proses (dikurangi 1 baris header)
jumlah_proses=$(($(ps aux | wc -l) - 1))
echo "Proses berjalan  : $jumlah_proses proses"

# 4. Deteksi virtualisasi
virt=$(sudo dmidecode -s system-product-name)
if [[ "$virt" == *"VirtualBox"* ]]; then
    status_virt="Terdeteksi (VirtualBox)"
else
    status_virt="Tidak terdeteksi"
fi
echo "Virtualisasi     : $status_virt"

# Fitur tambahan: Uptime VM
vm_uptime=$(uptime -p)
echo "Uptime VM        : $vm_uptime"
echo ""
echo "Menghitung metrik varian kelompok..."

# 5. Metrik 1: Disk usage
disk_usage=$(df -h / | awk 'NR==2 {print $5}' | tr -d '%')
echo "Disk usage       : ${disk_usage}%"

# 6. Metrik 2 (Varian E): nproc guest vs alokasi host
nproc_guest=$(nproc)
# alokasi_host diisi manual/dari luar, karena VBoxManage dijalankan di HOST bukan di VM
echo "vCPU guest       : $nproc_guest"


hasil_check=$(echo "$disk_usage $nproc_guest" | ./resource_check)
disk_status=$(echo "$hasil_check" | grep "Disk:" | awk '{print $2}')
vcpu_status=$(echo "$hasil_check" | grep "vCPU:" | awk '{print $2}')
echo "Disk Status      : $disk_status"
echo "vCPU Status      : $vcpu_status"
echo ""
echo "Menyimpan laporan ke sysinfo_report.txt..."

{
echo "=========================================================================="
echo "TUGAS 1 OS - KELOMPOK C4"
echo "=========================================================================="
printf "%-16s| %-18s| %-8s| %-25s\n" "Check Category" "Item" "Status" "Details"
echo "--------------------------------------------------------------------------"
printf "%-16s| %-18s| %-8s| %-25s\n" "OS" "$os_info" "PASS" "Kernel info"
printf "%-16s| %-18s| %-8s| %-25s\n" "Users" "Regular accounts" "PASS" "$jumlah_user akun"
printf "%-16s| %-18s| %-8s| %-25s\n" "Processes" "Running" "PASS" "$jumlah_proses proses"
printf "%-16s| %-18s| %-8s| %-25s\n" "Virtualization" "Hypervisor" "PASS" "$status_virt"
printf "%-16s| %-18s| %-8s| %-25s\n" "Uptime" "VM" "PASS" "$vm_uptime"
printf "%-16s| %-18s| %-8s| %-25s\n" "Disk" "${disk_usage}%" "$disk_status" "Threshold check"
printf "%-16s| %-18s| %-8s| %-25s\n" "vCPU" "guest:$nproc_guest" "$vcpu_status" "vs alokasi host"
echo "=========================================================================="
} > sysinfo_report.txt

echo "Laporan berhasil disimpan."
