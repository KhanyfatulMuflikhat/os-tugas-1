/*
 * scheduler.c  -  Simulasi CPU Scheduling: Multilevel Feedback Queue (MLFQ)
 *
 * Q0 = Round Robin (quantum q0)
 * Q1 = Round Robin (quantum q1)
 * Q2 = FCFS (tanpa quantum, jalan sampai selesai)
 *
 * ===== BAGIAN A (CORE ENGINE) ada di section "1" dan "2" =====
 * Section 4 = fungsi output, SATU FUNGSI PER BAGIAN SOAL. Tiap anggota hanya
 * mengedit fungsi print bagiannya (lihat tag "PIC" di komentar):
 *   A : engine, print_queue_migrations, print_context_switch
 *   B : print_process_input, print_gantt
 *   C : print_scheduling_table, print_averages, print_cpu_util_throughput
 *   D : print_process_states + skenario pengujian
 * Jangan ubah struct Process, run_mlfq(), dan fungsi log_...() tanpa
 * memberi tahu A.
 *
 * ATURAN DESAIN (jelaskan di video/laporan):
 *  1. Proses baru selalu masuk ke ujung Q0.
 *  2. Scheduler selalu ambil proses dari queue teratas yang tidak kosong.
 *  3. Jika quantum habis dan proses belum selesai -> DEMOTE (Q0->Q1, Q1->Q2).
 *  4. Q2 (FCFS) tidak punya quantum, hanya bisa berhenti karena selesai/preempt.
 *  5. PREEMPTIVE antar queue: proses yang baru tiba di Q0 langsung merebut CPU
 *     dari proses yang sedang jalan di Q1/Q2. Proses yang di-preempt kembali ke
 *     DEPAN queue-nya sendiri (urutan tidak berubah) dan quantum dihitung ulang.
 *  6. Jika proses tiba tepat saat quantum proses lain habis, proses yang tiba
 *     dimasukkan ke queue DULU, baru proses yang di-demote.
 *  7. (Opsional) Priority boost: tiap boost_period, semua proses Q1/Q2 naik ke Q0.
 *  8. Context switch = CPU pindah dari proses X ke proses Y (X != Y).
 *     Idle di antara dua proses tidak dihitung switch jika prosesnya sama.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_P    100
#define NUM_Q    3
#define MAX_LOG  5000

/* ------------------------------------------------------------------ */
/* 1. STRUCT & QUEUE                                                   */
/* ------------------------------------------------------------------ */

typedef enum { ST_NEW, ST_READY, ST_RUNNING, ST_TERMINATED } State;

typedef struct {
    int   pid;
    int   at, bt;        /* arrival time, burst time               */
    int   remaining;     /* sisa burst                             */
    int   first_start;   /* -1 jika belum pernah jalan (untuk RT)  */
    int   ct;            /* completion time                        */
    int   queue_level;   /* 0,1,2: posisi queue saat ini           */
    State state;
} Process;

/* Queue melingkar (deque): bisa push belakang (normal) & push depan (preempt) */
typedef struct {
    Process *items[MAX_P];
    int head, count;
} Queue;

static void q_push_back(Queue *q, Process *p) {
    q->items[(q->head + q->count) % MAX_P] = p;
    q->count++;
}
static void q_push_front(Queue *q, Process *p) {
    q->head = (q->head - 1 + MAX_P) % MAX_P;
    q->items[q->head] = p;
    q->count++;
}
static Process *q_pop(Queue *q) {
    Process *p = q->items[q->head];
    q->head = (q->head + 1) % MAX_P;
    q->count--;
    return p;
}
static int q_empty(Queue *q) { return q->count == 0; }

/* ------------------------------------------------------------------ */
/* 3. LOGGING (versi minimal; dipakai engine A)                        */
/* ------------------------------------------------------------------ */

typedef struct { int pid; State st; int t; } StateEvent;
typedef struct { int t, pid, from, to, remaining; } Migration;
typedef struct { int pid, start, end; } GanttSeg;   /* pid = -1 -> IDLE */

static StateEvent  state_log[MAX_LOG];  static int n_state_log = 0;
static Migration   mig_log[MAX_LOG];    static int n_mig = 0;
static GanttSeg    gantt[MAX_LOG];      static int n_gantt = 0;

static int cs_total = 0, cs_per_queue[NUM_Q] = {0, 0, 0};
static int preempt_count = 0;

static void log_state(int pid, State st, int t) {
    state_log[n_state_log++] = (StateEvent){pid, st, t};
}

static void log_migration(int t, int pid, int from, int to, int remaining) {
    mig_log[n_mig++] = (Migration){t, pid, from, to, remaining};
}

/* Catat 1 satuan waktu [t, t+1). Segmen berurutan dengan pid sama digabung. */
static void log_gantt(int pid, int t) {
    if (n_gantt > 0 && gantt[n_gantt - 1].pid == pid && gantt[n_gantt - 1].end == t) {
        gantt[n_gantt - 1].end = t + 1;
    } else {
        gantt[n_gantt++] = (GanttSeg){pid, t, t + 1};
    }
}

/* queue = queue tujuan proses yang mendapat CPU */
static void log_context_switch(int from_pid, int to_pid, int queue) {
    (void)from_pid; (void)to_pid;
    cs_total++;
    cs_per_queue[queue]++;
}

/* ------------------------------------------------------------------ */
/* 2. CORE MLFQ ENGINE  (BAGIAN A)                                     */
/* ------------------------------------------------------------------ */

static Queue queues[NUM_Q];

/* Masukkan semua proses yang sudah tiba (state NEW dan at <= t) ke ujung Q0.
 * Dipanggil sebelum demote supaya urutan arrival-vs-demote benar (aturan 6). */
static void admit_arrivals(Process procs[], int n, int t) {
    for (int i = 0; i < n; i++) {
        if (procs[i].state == ST_NEW && procs[i].at <= t) {
            procs[i].state = ST_READY;
            procs[i].queue_level = 0;
            q_push_back(&queues[0], &procs[i]);
            log_state(procs[i].pid, ST_READY, procs[i].at);
        }
    }
}

/* Turunkan proses dari queue 'from' ke queue 'from+1' (quantum habis). */
static void demote(Process *p, int t) {
    int from = p->queue_level, to = from + 1;
    p->queue_level = to;
    p->state = ST_READY;
    q_push_back(&queues[to], p);
    log_migration(t, p->pid, from, to, p->remaining);
    log_state(p->pid, ST_READY, t);
}

/* Priority boost: semua proses di Q1 dan Q2 (termasuk yang sedang jalan)
 * kembali ke Q0. */
static void priority_boost(Process *running, int t) {
    for (int lv = 1; lv < NUM_Q; lv++) {
        while (!q_empty(&queues[lv])) {
            Process *p = q_pop(&queues[lv]);
            log_migration(t, p->pid, lv, 0, p->remaining);
            p->queue_level = 0;
            q_push_back(&queues[0], p);
        }
    }
    if (running != NULL && running->queue_level > 0) {
        log_migration(t, running->pid, running->queue_level, 0, running->remaining);
        running->queue_level = 0;
    }
}

/* Jalankan simulasi.
 * quantum[0], quantum[1] = time quantum Q0 dan Q1 (quantum[2] tidak dipakai).
 * boost_period = 0 -> aging dimatikan.
 * Mengembalikan total waktu simulasi (waktu selesai proses terakhir). */
static int run_mlfq(Process procs[], int n, int quantum[NUM_Q], int boost_period,
                    int *busy_time) {
    int t = 0, done = 0;
    int last_pid = -1;           /* proses terakhir yang memegang CPU */
    int slice_used = 0;          /* lama proses berjalan pada giliran ini */
    Process *running = NULL;
    *busy_time = 0;

    while (done < n) {
        /* (a) proses yang tiba pada waktu t masuk Q0 */
        admit_arrivals(procs, n, t);

        /* (b) aging / priority boost (opsional) */
        if (boost_period > 0 && t > 0 && t % boost_period == 0)
            priority_boost(running, t);

        /* (c) PREEMPSI: ada proses di Q0, sedangkan yang jalan dari Q1/Q2 */
        if (running != NULL && running->queue_level > 0 && !q_empty(&queues[0])) {
            running->state = ST_READY;
            q_push_front(&queues[running->queue_level], running); /* tetap di depan */
            log_state(running->pid, ST_READY, t);
            preempt_count++;
            running = NULL;
        }

        /* (d) pilih proses berikutnya dari queue teratas yang tidak kosong */
        if (running == NULL) {
            int lv = -1;
            for (int i = 0; i < NUM_Q; i++)
                if (!q_empty(&queues[i])) { lv = i; break; }

            if (lv == -1) {              /* semua queue kosong -> CPU idle */
                log_gantt(-1, t);
                t++;
                continue;
            }

            running = q_pop(&queues[lv]);
            running->state = ST_RUNNING;
            log_state(running->pid, ST_RUNNING, t);
            if (running->first_start == -1) running->first_start = t;
            if (last_pid != -1 && last_pid != running->pid)
                log_context_switch(last_pid, running->pid, running->queue_level);
            slice_used = 0;
        }

        /* (e) jalankan 1 satuan waktu */
        running->remaining--;
        slice_used++;
        (*busy_time)++;
        log_gantt(running->pid, t);
        last_pid = running->pid;
        t++;

        /* (f) cek hasil: selesai? quantum habis? */
        if (running->remaining == 0) {
            running->ct = t;
            running->state = ST_TERMINATED;
            log_state(running->pid, ST_TERMINATED, t);
            done++;
            running = NULL;
        } else if (running->queue_level < NUM_Q - 1 &&
                   slice_used >= quantum[running->queue_level]) {
            admit_arrivals(procs, n, t);   /* arrival masuk dulu (aturan 6) */
            demote(running, t);
            running = NULL;
        }
    }
    return t;
}

/* ------------------------------------------------------------------ */
/* 4. OUTPUT -- satu fungsi per bagian, tiap orang edit fungsinya sendiri */
/*    Jangan ubah run_mlfq() / struct / fungsi log_...().              */
/* ------------------------------------------------------------------ */

#define LINE "==================================================\n"

static const char *state_name(State s) {
    switch (s) {
        case ST_NEW:     return "NEW";
        case ST_READY:   return "READY";
        case ST_RUNNING: return "RUNNING";
        default:         return "TERMINATED";
    }
}

/* ---------- BAGIAN 1 : Process Input  (PIC: B) ---------- */
static void print_process_input(Process procs[], int n, int quantum[NUM_Q]) {
    printf("\n" LINE "PROCESS INPUT  (Q0 quantum=%d, Q1 quantum=%d, Q2=FCFS)\n" LINE,
           quantum[0], quantum[1]);
    printf("PID\tAT\tBT\n");
    for (int i = 0; i < n; i++)
        printf("P%d\t%d\t%d\n", procs[i].pid, procs[i].at, procs[i].bt);
}

/* ---------- BAGIAN 2 : Gantt Chart  (PIC: B) ---------- */
static void print_gantt(void) {
    printf("\n" LINE "CPU EXECUTION TIMELINE\n" LINE);
    for (int i = 0; i < n_gantt; i++) {
        if (gantt[i].pid == -1) printf("| IDLE ");
        else                    printf("| P%-3d ", gantt[i].pid);
    }
    printf("|\n");
    for (int i = 0; i < n_gantt; i++) printf("%-7d", gantt[i].start);
    printf("%d\n", gantt[n_gantt - 1].end);
}

/* ---------- VARIAN MLFQ : Queue Migration  (PIC: A) ---------- */
static void print_queue_migrations(void) {
    printf("\n" LINE "QUEUE MIGRATIONS\n" LINE);
    for (int i = 0; i < n_mig; i++) {
        if (mig_log[i].to == 0)   /* naik ke Q0 = priority boost */
            printf("t=%d : P%d Q%d -> Q0 (priority boost; sisa BT=%d)\n", mig_log[i].t,
                   mig_log[i].pid, mig_log[i].from, mig_log[i].remaining);
        else
            printf("t=%d : P%d Q%d -> Q%d (quantum Q%d habis; sisa BT=%d)\n", mig_log[i].t,
                   mig_log[i].pid, mig_log[i].from, mig_log[i].to, mig_log[i].from,
                   mig_log[i].remaining);
    }
    printf("Total Queue Migration : %d\n", n_mig);
}

/* ---------- BAGIAN 3 : Scheduling Table  (PIC: C) ---------- */
static void print_scheduling_table(Process procs[], int n) {
    printf("\n" LINE "SCHEDULING TABLE\n" LINE);
    printf("PID\tAT\tBT\tCT\tTAT\tWT\tRT\n");
    for (int i = 0; i < n; i++) {
        int tat = procs[i].ct - procs[i].at;                 /* TAT = CT - AT */
        int wt  = tat - procs[i].bt;                         /* WT  = TAT - BT */
        int rt  = procs[i].first_start - procs[i].at;        /* RT  = start - AT */
        printf("P%d\t%d\t%d\t%d\t%d\t%d\t%d\n", procs[i].pid, procs[i].at, procs[i].bt,
               procs[i].ct, tat, wt, rt);
    }
}

/* ---------- BAGIAN 4 : Rata-rata  (PIC: C) ---------- */
static void print_averages(Process procs[], int n) {
    double swt = 0, stat = 0, srt = 0;
    for (int i = 0; i < n; i++) {
        int tat = procs[i].ct - procs[i].at;
        swt  += tat - procs[i].bt;
        stat += tat;
        srt  += procs[i].first_start - procs[i].at;
    }
    printf("\n" LINE "SCHEDULING PERFORMANCE\n" LINE);
    printf("Average Waiting Time    : %.2f\n", swt / n);
    printf("Average Turnaround Time : %.2f\n", stat / n);
    printf("Average Response Time   : %.2f\n", srt / n);
}

/* ---------- BAGIAN 5 : CPU Utilization & Throughput  (PIC: C) ---------- */
static void print_cpu_util_throughput(int n, int total_time, int busy_time) {
    printf("\n" LINE "CPU UTILIZATION AND THROUGHPUT\n" LINE);
    printf("CPU Utilization : %.2f%%\n", 100.0 * busy_time / total_time);
    printf("Throughput      : %.2f process/time unit\n", (double)n / total_time);
}

/* ---------- BAGIAN 6 : Context Switch  (PIC: A) ---------- */
static void print_context_switch(void) {
    printf("\n" LINE "CONTEXT SWITCH INFORMATION\n" LINE);
    printf("Total Context Switch : %d\n", cs_total);
    printf("  Breakdown per queue (queue tujuan): Q0=%d, Q1=%d, Q2=%d\n",
           cs_per_queue[0], cs_per_queue[1], cs_per_queue[2]);
    printf("Total Preemption     : %d\n", preempt_count);
}

/* ---------- BAGIAN 7 : Process State  (PIC: D) ---------- */
static void print_process_states(Process procs[], int n) {
    printf("\n" LINE "PROCESS STATE TRANSITIONS\n" LINE);
    for (int i = 0; i < n; i++) {
        printf("P%d : NEW", procs[i].pid);
        for (int k = 0; k < n_state_log; k++)
            if (state_log[k].pid == procs[i].pid)
                printf(" -> %s (t=%d)", state_name(state_log[k].st), state_log[k].t);
        printf("\n");
    }
}

/* Urutan tampilan akhir. Cukup tambah/ubah baris di sini kalau perlu. */
static void print_all(Process procs[], int n, int quantum[NUM_Q], int total_time,
                      int busy_time) {
    print_process_input(procs, n, quantum);
    print_gantt();
    print_queue_migrations();
    print_scheduling_table(procs, n);
    print_averages(procs, n);
    print_cpu_util_throughput(n, total_time, busy_time);
    print_context_switch();
    print_process_states(procs, n);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    Process procs[MAX_P];
    int n, quantum[NUM_Q] = {0, 0, 0}, boost_period;

    printf("Jumlah proses            : ");
    if (scanf("%d", &n) != 1 || n < 1 || n > MAX_P) { printf("Input tidak valid\n"); return 1; }
    printf("Time Quantum Q0, Q1      : ");
    if (scanf("%d %d", &quantum[0], &quantum[1]) != 2 || quantum[0] < 1 || quantum[1] < 1) {
        printf("Quantum harus >= 1\n"); return 1;
    }
    printf("Periode boost (0=mati)   : ");
    if (scanf("%d", &boost_period) != 1 || boost_period < 0) return 1;

    for (int i = 0; i < n; i++) {
        int at, bt;
        printf("P%d  Arrival Time, Burst Time : ", i + 1);
        if (scanf("%d %d", &at, &bt) != 2 || at < 0 || bt < 1) {
            printf("AT harus >= 0 dan BT >= 1\n"); return 1;
        }
        procs[i] = (Process){ .pid = i + 1, .at = at, .bt = bt, .remaining = bt,
                              .first_start = -1, .ct = 0, .queue_level = 0, .state = ST_NEW };
    }

    int busy;
    int total = run_mlfq(procs, n, quantum, boost_period, &busy);
    print_all(procs, n, quantum, total, busy);
    return 0;
}