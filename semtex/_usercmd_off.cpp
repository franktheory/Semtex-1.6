#include <cstdio>
#include <cstddef>
typedef unsigned char byte;
struct usercmd_t {
    short lerp_msec;
    byte msec;
    byte pad0;
    float viewangles[3];
    float forwardmove;
    float sidemove;
    float upmove;
    byte lightlevel;
    unsigned short buttons;
    byte impulse;
    byte weaponselect;
    int impact_index;
    float impact_position[3];
};
int main() {
    printf("viewangles %zu buttons %zu size %zu\n", offsetof(usercmd_t,viewangles), offsetof(usercmd_t,buttons), sizeof(usercmd_t));
    return 0;
}
