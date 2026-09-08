#include <cstdio>
#include <cstdint>

int main() {
    uint32_t s = 0x8D19C16D, d = 0x13B34517, a = 0x8D;
    // channels: byte0=B byte1=G byte2=R byte3=A
    uint32_t d1 = d & 0x00ff00ff; // R and B packed
    uint32_t d2 = d & 0x0000ff00; // G packed
    uint32_t s1 = s & 0x00ff00ff;
    uint32_t s2 = s & 0x0000ff00;

    uint32_t X = s1 - d1;         // 32-bit packed R+B subtraction (borrow across)
    uint32_t t1 = ((X * a) >> 8) + d1; // 32-bit
    uint32_t r1 = t1 & 0x00ff00ff;
    uint32_t rR = (r1 >> 16) & 0xff;   // red byte
    uint32_t rB = r1 & 0xff;           // blue byte

    uint32_t Xg = s2 - d2;        // 32-bit packed G
    uint32_t t2 = ((Xg * a) >> 8) + d2;
    uint32_t rG = (t2 >> 8) & 0xff;    // green byte

    printf("s=%08X d=%08X a=%02X\n", s, d, a);
    printf("s1(R+B)=%08X d1=%08X X=%08X  (s1-d1)\n", s1, d1, X);
    printf("X*a=%08X (mod2^32) then (X*a)>>8=%08X +d1=%08X &mask=%08X\n",
           (uint32_t)(X * a), ((X*a)>>8), t1, r1);
    printf("  -> R=%02X B=%02X\n", rR, rB);
    printf("s2(G)=%08X d2=%08X Xg=%08X Xg*a=%08X >>8=%08X +d2=%08X G=%02X\n",
           s2, d2, Xg, (uint32_t)(Xg*a), ((Xg*a)>>8), t2, rG);
    printf("full scalar=%08X\n", (r1 | (t2 & 0x0000ff00)));
    return 0;
}