#include "../include/digital_input.h"
#include <assert.h>
#include <stdio.h>
int main()
{
    unsigned char physical[128] = {}, out[161];
    uint32_t pov[4] = {0xffffffffu, 0xffffu, 0xffffffffu, 0xffffffffu};
    physical[0]=physical[127]=0x80;
    for(int i=0;i<161;++i)out[i]=42;
    WheelDigitalButtons(physical,pov,out,160);
    assert(out[0]==1&&out[127]==1&&out[160]==42);
    for(int i=128;i<160;++i)assert(out[i]==0);
    for(int hat=0;hat<4;++hat)for(int direction=0;direction<8;++direction){
        pov[hat]=(uint32_t)direction*4500u;
        WheelDigitalButtons(physical,pov,out,160);
        for(int d=0;d<8;++d)assert(out[128+hat*8+d]==(d==direction));
        pov[hat]=0xffffffffu;
        WheelDigitalButtons(physical,pov,out,160);
        for(int d=0;d<8;++d)assert(out[128+hat*8+d]==0);
    }
    assert(WheelPovDirection(35999)==0&&WheelPovDirection(36000)==-1);
    assert(WheelPovDirection(2249)==0&&WheelPovDirection(2250)==1);
    for(int i=0;i<161;++i)out[i]=42;
    WheelDigitalButtons(physical,pov,out,128);
    assert(out[127]==1&&out[128]==42);
    WheelDigitalButtons(physical,pov,out,0);assert(out[128]==42);
    puts("PASS digital input: 4 hats x 8 directions, release/neutral, physical 127, boundaries and legacy buffer bounds");
}
