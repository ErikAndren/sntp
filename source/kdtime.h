#include <stdint.h>
#include <stdbool.h>
#include <time.h>

int KD_Init();
int KD_Close();

int KD_RefreshRTCCounter(bool save_flag);
int KD_SetUniversalTime(time_t time, uint32_t save_flag);
int KD_GetUniversalTime(time_t* time);
int KD_GetTimeDifference(uint64_t* diff);
