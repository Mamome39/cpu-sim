/*
 * Sieve of Eratosthenes benchmark for cpu-sim.
 *
 * Marks composites in a byte array up to N, then counts primes.
 * Control-flow heavy: tight branch back-edges and byte-wide
 * sb/lbu accesses that the word-only benchmarks skip. No
 * multiply is used (the inner step advances by addition).
 *
 * Returns the number of primes below N in a0.
 * (primes below 2000 = 303)
 */

#define N 2000

static unsigned char is_composite[N];

int main(void) {
    int count = 0;
    for (int i = 2; i < N; i++) {
        if (!is_composite[i]) {
            count++;
            for (int j = i + i; j < N; j += i)
                is_composite[j] = 1;
        }
    }
    return count;   /* 303 for N = 2000 */
}
