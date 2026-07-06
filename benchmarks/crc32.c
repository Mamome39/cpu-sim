/*
 * CRC32 benchmark for cpu-sim.
 *
 * Bit-manipulation heavy: builds the 256-entry lookup table at
 * runtime (8 shift/xor rounds per entry), then CRCs a fixed
 * message byte-wise. Exercises srl/xor/andi, lbu input reads,
 * and computed-address table loads — none of which the other
 * benchmarks stress.
 *
 * Result is the CRC32 of the message, returned in a0.
 */

static unsigned int table[256];

static void build_table(void) {
    for (unsigned int i = 0; i < 256; i++) {
        unsigned int c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        table[i] = c;
    }
}

static unsigned int crc32(const unsigned char* buf, int len) {
    unsigned int c = 0xFFFFFFFFu;
    for (int i = 0; i < len; i++)
        c = table[(c ^ buf[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static const unsigned char msg[] =
    "The quick brown fox jumps over the lazy dog";

int main(void) {
    build_table();
    /* sizeof - 1 to drop the trailing NUL terminator */
    return (int)crc32(msg, sizeof(msg) - 1);
}
