#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

void InitSound(void);
void SndFreq(uint64_t f);
double GetVolume(void);
void SetVolume(double);

#ifdef __cplusplus
}
#endif
