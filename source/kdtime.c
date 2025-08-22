#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ogc/ipc.h>

#include "kdtime.h"

#define errorf(fmt, ...) { fprintf(stderr, "[%s] " fmt "\n", __FUNCTION__, ##__VA_ARGS__); }

struct kd_time {
    int fd;
    union {
        uint32_t buffer[8];

        struct {
            time_t time;
            uint32_t save_flag;
        } universaltime;

        struct {
            uint32_t value;
            uint32_t: 31;
            uint32_t save_flag: 1;
        } rtc;
    } args;

    union {
        int ret;
        uint32_t buffer[8];

        struct {
            time_t time;
        } universaltime;

        struct {
            uint64_t value;
        } timedifference;
    } result;
};

static struct kd_time kd_time = {
    .fd = IPC_ENOENT,
};

int KD_Init() {
    int ret = 0;

    if (kd_time.fd >= 0)
        return 0;

    for (int tries = 5; tries > 0; tries--) {
        ret = kd_time.fd = IOS_Open("/dev/net/kd/time", 0);
        if (ret == IPC_ENOENT) {
            usleep(5000);
        }
        else {
            break;
        }
    }

    if (kd_time.fd < 0) {
        errorf("IOS_Open(/dev/net/kd/time) ret=%i\n", ret);
        return ret;
    }

    ret = KD_RefreshRTCCounter(false);

    return ret;
}

int KD_Close() {
    int ret = 0;

    if (kd_time.fd >= 0) {
        ret = IOS_Close(kd_time.fd);
        kd_time.fd = IPC_ENOENT;
    }

    return ret;
}

static int _KD_SendCommand(int no) {
    int ret = IOS_Ioctl(kd_time.fd, no, kd_time.args.buffer, sizeof(kd_time.args.buffer), kd_time.result.buffer, sizeof(kd_time.result.buffer));
    if (ret < 0) {
        errorf("IPC error [cmd=%#x, ret=%i]", no, ret);
        return ret;
    }

    return kd_time.result.ret;
}

extern uint32_t __SYS_GetRTC(uint32_t* rtc);

int KD_RefreshRTCCounter(bool save_flag) {
    if (kd_time.fd < 0)
        return -10;

    if (!__SYS_GetRTC(&kd_time.args.rtc.value)) {
        errorf("__SYS_GetRTC failure");
        return -1;
    }

    kd_time.args.rtc.save_flag = save_flag;
    return _KD_SendCommand(0x17);
}

int KD_SetUniversalTime(time_t time, uint32_t save_flag) {
    if (kd_time.fd < 0)
        return -10;

    kd_time.args.universaltime.time = time;
    kd_time.args.universaltime.save_flag = save_flag;

    return _KD_SendCommand(0x15);
}

// pretty much just time(2)
int KD_GetUniversalTime(time_t* time) {
    if (kd_time.fd < 0)
        return -10;

    int ret = _KD_SendCommand(0x14);
    if (ret == 0 && time != NULL)
        *time = kd_time.result.universaltime.time;

    return ret;
}

int KD_GetTimeDifference(uint64_t* diff) {
    if (kd_time.fd < 0)
        return -10;

    int ret = _KD_SendCommand(0x18);
    if (ret == 0 && diff != NULL)
        *diff = kd_time.result.timedifference.value;

    return ret;
}
