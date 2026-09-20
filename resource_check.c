#include <stdio.h>

#define ALOKASI_HOST 2
#define AMBANG_BATAS_FAIL 90
#define AMBANG_BATAS_WARN 75

int main() {
    int disk_usage;
    int vcpu_guest;

    scanf("%d %d", &disk_usage, &vcpu_guest);

    // Metrik 1: mengecek disk
    printf("Disk: ");
    if (disk_usage >= AMBANG_BATAS_FAIL) {
        printf("FAIL\n");
    } else if (disk_usage >= AMBANG_BATAS_WARN) {
        printf("WARN\n");
    } else {
        printf("PASS\n");
    }

    // Metrik 2: mengecek vcpu
    if (ALOKASI_HOST == vcpu_guest) {
        printf("vCPU: PASS\n");
    } else {
        printf("vCPU: FAIL\n");
    }

    return 0;
}