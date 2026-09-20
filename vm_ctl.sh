ACTION="$1"
ARG2="$2"
ARG3="$3"
ARG4="$4"

# Mencetak header untuk setiap output
print_header() {
    echo "===================================="
    echo "      TUGAS 1 OS - KELOMPOK C4"
    echo "===================================="
}

# Menampilkan semua VM yang terdaftar di VirtualBox
vm_list() {
    echo "Memindai daftar Virtual Machine..."
    echo ""
    echo "Daftar VM terdaftar:"
    VBoxManage list vms | sed 's/{.*}//' | tr -d '"\r' | nl -w2 -s'. '
}

# Menampilkan RAM, jumlah vCPU, dan status VM
vm_info() {
    local VM_NAME="$1"

    if [ -z "$VM_NAME" ]; then
        echo "Error: nama VM belum diisi. Gunakan: ./vm_ctl.sh info <nama_vm>"
        return 1
    fi

    RAW=$(VBoxManage showvminfo "$VM_NAME" --machinereadable 2>/dev/null | tr -d '\r')

    if [ -z "$RAW" ]; then
        echo "Error: VM '$VM_NAME' tidak ditemukan."
        return 1
    fi

    RAM=$(echo "$RAW" | grep "^memory=" | cut -d= -f2)
    CPU=$(echo "$RAW" | grep "^cpus=" | cut -d= -f2)
    STATE=$(echo "$RAW" | grep "^VMState=" | cut -d= -f2 | tr -d '"')

    if [ "$STATE" = "poweroff" ]; then
        STATE="powered off"
    fi

    echo "VM                    : $VM_NAME"
    echo "RAM dialokasikan      : ${RAM} MB"
    echo "vCPU dialokasikan     : ${CPU}"
    echo "Status saat ini       : ${STATE}"
}

# Menyalakan VM (headless)
vm_start() {
    local VM_NAME="$1"

    if [ -z "$VM_NAME" ]; then
        echo "Error: nama VM belum diisi. Gunakan: ./vm_ctl.sh start <nama_vm>"
        return 1
    fi

    if ! vm_exists "$VM_NAME"; then
        echo "Error: VM '$VM_NAME' tidak ditemukan. Cek dengan: ./vm_ctl.sh list"
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

# Mematikan VM
vm_stop() {
    local VM_NAME="$1"

    if [ -z "$VM_NAME" ]; then
        echo "Error: nama VM belum diisi. Gunakan: ./vm_ctl.sh stop <nama_vm>"
        return 1
    fi

    if ! vm_exists "$VM_NAME"; then
        echo "Error: VM '$VM_NAME' tidak ditemukan. Cek dengan: ./vm_ctl.sh list"
        return 1
    fi

    echo "Mematikan VM '$VM_NAME' secara aman..."
    VBoxManage controlvm "$VM_NAME" acpipowerbutton

    if [ $? -ne 0 ]; then
        echo "Gagal mengirim perintah shutdown ke VM '$VM_NAME'."
        return 1
    fi
    echo "Perintah shutdown terkirim. Menunggu VM benar-benar mati..."

    local MAX_WAIT=60  
    local WAITED=0
    local STATE=""

    while [ $WAITED -lt $MAX_WAIT ]; do
        STATE=$(VBoxManage showvminfo "$VM_NAME" --machinereadable | grep "^VMState=" | cut -d= -f2 | tr -d '"\r')
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

# Cek eksistensi VM
vm_exists() {
    local vm="$1"
    VBoxManage list vms | tr -d '\r' | grep -q "^\"$vm\" "
}

# Membuat snapshot
vm_snapshot_create() {
    local vm="$1"
    local snap="$2"

    if [ -z "$vm" ] || [ -z "$snap" ]; then
        echo "Penggunaan: ./vm_ctl.sh snapshot create <nama_vm> <nama_snapshot>"
        return 1
    fi

    if ! vm_exists "$vm"; then
        echo "Error: VM '$vm' tidak ditemukan. Cek dengan: ./vm_ctl.sh list"
        return 1
    fi

    echo "Membuat snapshot '$snap' pada VM '$vm'..."
    if VBoxManage snapshot "$vm" take "$snap" > /dev/null 2>&1; then
        echo "Snapshot '$snap' berhasil dibuat pada $(date '+%Y-%m-%d %H:%M:%S')."
    else
        echo "Error: gagal membuat snapshot."
        return 1
    fi
}

# Menampilkan snapshot list
vm_snapshot_list() {
    local vm="$1"

    if [ -z "$vm" ]; then
        echo "Penggunaan: ./vm_ctl.sh snapshot list <nama_vm>"
        return 1
    fi

    if ! vm_exists "$vm"; then
        echo "Error: VM '$vm' tidak ditemukan. Cek dengan: ./vm_ctl.sh list"
        return 1
    fi

    local output
    local output
    output=$(VBoxManage snapshot "$vm" list --machinereadable 2>/dev/null | tr -d '\r' | grep '^SnapshotName')

    if [ -z "$output" ]; then
        echo "VM '$vm' belum memiliki snapshot."
        return 0
    fi

    echo "Daftar snapshot VM '$vm':"
    local i=1
    while IFS= read -r name; do
        echo "   $i. $name"
        i=$((i + 1))
    done < <(echo "$output" | grep '^SnapshotName' | sed 's/^SnapshotName[^=]*="\(.*\)"$/\1/')
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
    snapshot)
        print_header
        case "$ARG2" in
            create) vm_snapshot_create "$ARG3" "$ARG4" ;;
            list)   vm_snapshot_list "$ARG3" ;;
            *)      echo "Subcommand snapshot: create | list" ;;
        esac
        ;;
    *)
        echo "Command tidak dikenali."
        echo "Gunakan: list | info <vm> | start <vm> | stop <vm> | snapshot create <vm> <nama> | snapshot list <vm>"

        ;;
esac

