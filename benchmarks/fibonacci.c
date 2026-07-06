/*
 * Recursive Fibonacci benchmark for cpu-sim.
 *
 * Stresses function call/return: every call spills ra (and a
 * callee-saved reg) to the stack, so this exercises jal/jalr,
 * stack loads/stores, and deep recursion the loops-only
 * benchmarks never touch.
 *
 * fib(24) = 46368, returned in a0.
 */

static int fib(int n) {
    if (n < 2)
        return n;
    return fib(n - 1) + fib(n - 2);
}

int main(void) {
    return fib(24);   /* 46368 */
}
