/*-------------------------------------------------------------

sysconf.h -- SYSCONF & setting.txt support

Copyright (C) 2008 tona

Original conf.c portions written by and Copright (C) 2008
Hector Martin (marcan)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#ifndef __SYSCONF_H__
#define __SYSCONF_H__

#if defined(HW_RVL)

#include <gctypes.h>
#include <gcutil.h>

#define SYSCONF_EBADFILE	-0x6001
#define SYSCONF_ENOENT		-0x6002
#define SYSCONF_ETOOBIG	-0x6003
#define SYSCONF_ENOTINIT	-0x6004
#define SYSCONF_ENOTIMPL	-0x6005
#define SYSCONF_EBADVALUE	-0x6006
#define SYSCONF_ENOMEM		-0x6007
#define SYSCONF_EPERMS	-0x6008
#define SYSCONF_EBADWRITE	-0x6009
#define SYSCONF_ERR_OK		0

//#define DEBUG_SYSCONF

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

enum {
	SYSCONF_BIGARRAY = 1,
	SYSCONF_SMALLARRAY,
	SYSCONF_BYTE,
	SYSCONF_SHORT,
	SYSCONF_LONG,
	SYSCONF_BOOL = 7
};

enum {
	SYSCONF_VIDEO_NTSC = 0,
	SYSCONF_VIDEO_PAL,
	SYSCONF_VIDEO_MPAL
};

enum {
	SYSCONF_REGION_JP = 0,
	SYSCONF_REGION_US,
	SYSCONF_REGION_EU
};

enum {
	SYSCONF_AREA_JPN = 0,
	SYSCONF_AREA_USA,
	SYSCONF_AREA_EUR,
	SYSCONF_AREA_AUS,
	SYSCONF_AREA_BRA,
	SYSCONF_AREA_TWN,
	SYSCONF_AREA_ROC,
	SYSCONF_AREA_KOR,
	SYSCONF_AREA_HKG,
	SYSCONF_AREA_ASI,
	SYSCONF_AREA_LTN,
	SYSCONF_AREA_SAF
};

enum {
	SYSCONF_SHUTDOWN_STANDBY = 0,
	SYSCONF_SHUTDOWN_IDLE
};

enum {
	SYSCONF_LED_OFF = 0,
	SYSCONF_LED_DIM,
	SYSCONF_LED_BRIGHT
};

enum {
	SYSCONF_SOUND_MONO = 0,
	SYSCONF_SOUND_STEREO,
	SYSCONF_SOUND_SURROUND
};

enum {
	SYSCONF_LANG_JAPANESE = 0,
	SYSCONF_LANG_ENGLISH,
	SYSCONF_LANG_GERMAN,
	SYSCONF_LANG_FRENCH,
	SYSCONF_LANG_SPANISH,
	SYSCONF_LANG_ITALIAN,
	SYSCONF_LANG_DUTCH
};

enum {
	SYSCONF_ASPECT_4_3 = 0,
	SYSCONF_ASPECT_16_9
};

enum {
	SYSCONF_SENSORBAR_BOTTOM = 0,
	SYSCONF_SENSORBAR_TOP
};

typedef struct _sysconf_pad_device sysconf_pad_device;

struct _sysconf_pad_device {
	u8 bdaddr[6];
	char name[0x40];
} ATTRIBUTE_PACKED;

#ifdef DEBUG_SYSCONF
void SYSCONF_DumpBuffer(void);
void SYSCONF_DumpTxtBuffer(void);
void SYSCONF_DumpEncryptedTxtBuffer(void);
void SYSCONF_PrintAllSettings(void);
#endif /* DEBUG_SYSCONF */

s32 SYSCONF_Init(void);
/* SYSCONF configuation */
s32 SYSCONF_GetLength(const char *name);
s32 SYSCONF_GetType(const char *name);
s32 SYSCONF_Get(const char *name, void *buffer, u32 length);
s32 SYSCONF_GetShutdownMode(void);
s32 SYSCONF_GetIdleLedMode(void);
s32 SYSCONF_GetProgressiveScan(void);
s32 SYSCONF_GetEuRGB60(void);
s32 SYSCONF_GetIRSensitivity(void);
s32 SYSCONF_GetSensorBarPosition(void);
s32 SYSCONF_GetPadSpeakerVolume(void);
s32 SYSCONF_GetPadMotorMode(void);
s32 SYSCONF_GetSoundMode(void);
s32 SYSCONF_GetLanguage(void);
s32 SYSCONF_GetCounterBias(u32 *bias);
s32 SYSCONF_GetScreenSaverMode(void);
s32 SYSCONF_GetDisplayOffsetH(s8 *offset);
s32 SYSCONF_GetPadDevices(sysconf_pad_device *devs, int count);
s32 SYSCONF_GetNickName(u8 *nickname);
s32 SYSCONF_GetAspectRatio(void);
s32 SYSCONF_GetEULA(void);
s32 SYSCONF_GetParentalPassword(s8 *password);
s32 SYSCONF_GetParentalAnswer(s8 *answer);
s32 SYSCONF_GetWiiConnect24(void);
/* setting.txt configuration */
s32 SYSCONF_GetRegion(void);
s32 SYSCONF_GetArea(void);
s32 SYSCONF_GetVideo(void);


/* Set functions */
s32 SYSCONF_SaveChanges(void);
s32 SYSCONF_Set(const char *name, const void *value, u32 length);


s32 SYSCONF_SetShutdownMode(u8 value);
s32 SYSCONF_SetIdleLedMode(u8 value);
s32 SYSCONF_SetProgressiveScan(u8 value);
s32 SYSCONF_SetEuRGB60(u8 value);
s32 SYSCONF_SetIRSensitivity(u32 value);
s32 SYSCONF_SetSensorBarPosition(u8 value);
s32 SYSCONF_SetPadSpeakerVolume(u8 value);
s32 SYSCONF_SetPadMotorMode(u8 value);
s32 SYSCONF_SetSoundMode(u8 value);
s32 SYSCONF_SetLanguage(u8 value);

s32 SYSCONF_SetCounterBias(u32 bias);
s32 SYSCONF_SetScreenSaverMode(u8 value);
s32 SYSCONF_SetDisplayOffsetH(s8 offset);
s32 SYSCONF_SetPadDevices(const sysconf_pad_device *devs, u8 count);
s32 SYSCONF_SetNickName(const u8 *nickname, u16 length);
s32 SYSCONF_SetAspectRatio(u8 value);
s32 SYSCONF_SetEULA(u8 value);
s32 SYSCONF_SetParentalPassword(const s8 *password, u32 length);
s32 SYSCONF_SetParentalAnswer(const s8 *answer, u32 length);
s32 SYSCONF_SetWiiConnect24(u32 value);

s32 SYSCONF_SetRegion(s32 value);
s32 SYSCONF_SetArea(s32 value);
s32 SYSCONF_SetVideo(s32 value);


#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif

#endif
