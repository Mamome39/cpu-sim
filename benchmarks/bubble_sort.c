/*
 * Bubble sort benchmark for cpu-sim.
 *
 * Sorts 64 integers in descending initial order (worst case for bubble
 * sort: maximises swaps and branch mispredicts).  Result in a0 after
 * return from main() is arr[0], which should be 1 after a correct sort.
 *
 * Compile:
 *   riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32 -O1 \
 *       -nostdlib -nostartfiles -T benchmarks/link.ld \
 *       benchmarks/start.S benchmarks/bubble_sort.c \
 *       -o benchmarks/bubble_sort.elf
 */

#define N 64

static int arr[N];

static void init_descending(void) {
    for (int i = 0; i < N; i++)
        arr[i] = N - i;   /* arr = [64, 63, 62, ..., 1] */
}

static void bubble_sort(void) {
    for (int i = 0; i < N - 1; i++) {
        for (int j = 0; j < N - 1 - i; j++) {
            if (arr[j] > arr[j + 1]) {
                int tmp   = arr[j];
                arr[j]    = arr[j + 1];
                arr[j + 1] = tmp;
            }
        }
    }
}

int main(void) {
    init_descending();
    bubble_sort();
    return arr[0];   /* 1 on correct sort */
}
