#pragma once
#include <stdint.h>

// Keep all 128 physical buttons. Four hats occupy eight exact directions each
// at 128..159 (N, NE, E, SE, S, SW, W, NW). Never alias physical buttons.
inline int WheelPovDirection(uint32_t angle)
{
    if ((angle & 0xffffu) == 0xffffu || angle >= 36000u) return -1;
    return (int)(((angle + 2250u) / 4500u) % 8u);
}
inline void WheelDigitalButtons(const unsigned char* raw, const uint32_t* pov,
                                unsigned char* output, int count)
{
    if (!output || count <= 0) return;
    const int n = count < 160 ? count : 160;
    for (int i = 0; i < n; ++i) {
        if (i < 128) output[i] = (raw[i] & 0x80) ? 1 : 0;
        else output[i] = WheelPovDirection(pov[(i - 128) / 8]) == (i - 128) % 8 ? 1 : 0;
    }
}
