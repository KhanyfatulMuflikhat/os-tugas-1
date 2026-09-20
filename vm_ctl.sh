ACTION="$1"
ARG2="$2"
ARG3="$3"
ARG4="$4"

print_header() {
    echo "===================================="
    echo "      TUGAS 1 OS - KELOMPOK C04"
    echo "===================================="
}

vm_list() {
    echo "Memindai daftar Virtual Machine..."
    echo ""
    echo "Daftar VM terdaftar:"
    VBoxManage list vms | sed 's/{.*}//' | tr -d '"' | nl -w2 -s'. '
}

vm_info() {
    local VM_NAME="$1"

    if [ -z "$VM_NAME" ]; then
        echo "Error: nama VM belum diisi. Gunakan: ./vm_ctl.sh info <nama_vm>"
        return 1
    fi

    RAW=$(VBoxManage showvminfo "$VM_NAME" --machinereadable 2>/dev/null)

    if [ -z "$RAW" ]; then
        echo "Error: VM '$VM_NAME' tidak ditemukan."
        return 1
    fi

    RAM=$(echo "$RAW" | grep "^memory=" | cut -d= -f2)
    CPU=$(echo "$RAW" | grep "^cpus=" | cut -d= -f2)
    STATE=$(echo "$RAW" | grep "^VMState=" | cut -d= -f2 | tr -d '"')

    echo "VM                    : $VM_NAME"
    echo "RAM dialokasikan      : ${RAM} MB"
    echo "vCPU dialokasikan     : ${CPU}"
    echo "Status saat ini       : ${STATE}"
}

vm_start() {
    local VM_NAME="$1"

    if [ -z "$VM_NAME" ]; then
        echo "Error: nama VM belum diisi. Gunakan: ./vm_ctl.sh start <nama_vm>"
        return 1
    fi

    echo "Menyalakan VM '$VM_NAME' secara headless..."
    VBoxManage startvm "$VM_NAME" --type headless

    if [ $? -eq 0 ]; then
        echo "VM '$VM_NAME' berhasil dinyalakan. Status: running"
    else
        echo "Gagal menyalakan VM '$VM_NAME'."
    fi
}

vm_stop() {
    local VM_NAME="$1"

    if [ -z "$VM_NAME" ]; then
        echo "Error: nama VM belum diisi. Gunakan: ./vm_ctl.sh stop <nama_vm>"
        return 1
    fi

    echo "Mematikan VM '$VM_NAME' secara aman..."
    VBoxManage controlvm "$VM_NAME" acpipowerbutton

    if [ $? -ne 0 ]; then
        echo "Gagal mengirim perintah shutdown ke VM '$VM_NAME'."
        return 1
    fi
    echo "Perintah shutdown terkirim. Menunggu VM benar-benar mati..."

    local MAX_WAIT=30  
    local WAITED=0
    local STATE=""

    while [ $WAITED -lt $MAX_WAIT ]; do
        STATE=$(VBoxManage showvminfo "$VM_NAME" --machinereadable | grep "^VMState=" | cut -d= -f2 | tr -d '"')
        if [ "$STATE" == "poweroff" ]; then
            echo "VM '$VM_NAME' berhasil dimatikan. Status: powered off"
            return 0
        fi
        sleep 2
        WAITED=$((WAITED + 2))
    done

    echo "Peringatan: VM '$VM_NAME' belum sepenuhnya mati setelah ${MAX_WAIT} detik."
    echo "Status saat ini: $STATE"
    echo "Kemungkinan penyebab: guest OS tidak merespons sinyal ACPI (misal acpid/logind tidak aktif)."
}

case "$ACTION" in
    list)
        print_header
        vm_list
        ;;
    info)
        print_header
        vm_info "$ARG2"
        ;;
    start)
        print_header
        vm_start "$ARG2"
        ;;
    stop)
        print_header
        vm_stop "$ARG2"
        ;;
    *)
        echo "Command tidak dikenali."
        echo "Gunakan: list | info <vm> | start <vm> | stop <vm>"
        ;;
esac
