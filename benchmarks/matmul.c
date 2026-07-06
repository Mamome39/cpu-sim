/*
 * Integer matrix-multiply benchmark for cpu-sim.
 *
 * Multiplies two 16x16 int matrices. RV32I has no hardware
 * multiply, so each a[i][k]*b[k][j] becomes a call into
 * libgcc's __mulsi3 — this benchmark exercises that software
 * routine alongside a dense triple-nested lw/sw inner loop.
 *
 * Returns c[15][15] in a0.
 */

#define N 16

static int a[N][N];
static int b[N][N];
static int c[N][N];

static void init(void) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            a[i][j] = i + j;
            b[i][j] = i - j;
        }
}

static void matmul(void) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            int sum = 0;
            for (int k = 0; k < N; k++)
                sum += a[i][k] * b[k][j];
            c[i][j] = sum;
        }
}

int main(void) {
    init();
    matmul();
    return c[N - 1][N - 1];
}
