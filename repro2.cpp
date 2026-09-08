#include <cstdio>
int main(){
    unsigned d = 0x13, s = 0x8D, a = 0x8D;
    // scalar ps_alpha: ((s-d)*a >>8)+d
    unsigned diff = s - d; // 0x7A
    unsigned r = ((diff * a) >> 8) + d;
    printf("diff=%X r=%02X (expected scalar 5E)\n", diff, r);
    // Why scalar=5E? Let's compute exactly
    unsigned d1 = d & 0xff; 
    unsigned res = ((((s - d1) * a) >> 8) + d1) & 0xff;
    printf("res=%02X\n", res);
    // (0x8D-0x13)=0x7A=122; 122*141=17202; 17202>>8=67=0x43; 0x43+0x13=0x56? that's not 5E
    // Let's be careful: a = 0x8D = 141
    unsigned t = ((s - d1) * a) >> 8; printf("t=%02X\n", t);
    return 0;
}
