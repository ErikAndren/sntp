/*-------------------------------------------------------------

sysconf.c -- SYSCONF & setting.txt support

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

#if defined(HW_RVL)

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <ogc/ipc.h>
#include <ogc/isfs.h>
#include <ogc/es.h>

#include "sysconf.h"

#define DEBUG_SYSCONF

#ifdef DEBUG_SYSCONF
#include "wiibasics.h"
#endif

static int __sysconf_inited = 0;
static int __sysconf_buffer_txt_decrypted = 0;
static u8 __sysconf_buffer[0x4000] ATTRIBUTE_ALIGN(32);
static char __sysconf_txt_buffer[0x101] ATTRIBUTE_ALIGN(32);
static int __sysconf_buffer_updated = 0;
static int __sysconf_txt_buffer_updated = 0;

static const char __sysconf_file[] ATTRIBUTE_ALIGN(32) = "/shared2/sys/SYSCONF";
//static const char __sysconf_txt_file[] ATTRIBUTE_ALIGN(32) = "/title/00000001/00000002/data/setting.txt";
static const char __sysconf_txt_file[] ATTRIBUTE_ALIGN(32) = "/title/00000001/00000002/data/setting.txt";

int __SYSCONF_EndOfTextOffset(void){
	int i;
	int offset = 0;
	
	for (i = 0; i < 0x100; i++)
		if (!memcmp(__sysconf_txt_buffer+i, "\r\n", 2))
			offset = i;
		
	offset += 2;
	return offset;
}

void __SYSCONF_DecryptEncryptTextBuffer(void)
{
	u32 key = 0x73B5DBFA;
	int i;
	char *end = (char*)__sysconf_txt_buffer;
	
	if (__sysconf_buffer_txt_decrypted)
		end += __SYSCONF_EndOfTextOffset();
	
	for(i=0; i<0x100; i++) {
		__sysconf_txt_buffer[i] ^= key & 0xff;
		key = (key<<1) | (key>>31);
	}
	
	__sysconf_buffer_txt_decrypted = !__sysconf_buffer_txt_decrypted;
	
	if (__sysconf_buffer_txt_decrypted)
		end += __SYSCONF_EndOfTextOffset();
	
	
	memset(end, 0, (__sysconf_txt_buffer+0x100)-end);
	
}

#ifdef DEBUG_SYSCONF

void SYSCONF_DumpBuffer(void){
	if(!__sysconf_inited) return;
	hex_print_array16(__sysconf_buffer, 0x4000);
}

void SYSCONF_DumpTxtBuffer(void){
	if(!__sysconf_inited) return;
	hex_print_array16((u8*)__sysconf_txt_buffer, 0x101);
}

void SYSCONF_DumpEncryptedTxtBuffer(void){
	if(!__sysconf_inited) return;
	int was = __sysconf_buffer_txt_decrypted;
	if (__sysconf_buffer_txt_decrypted)
		__SYSCONF_DecryptEncryptTextBuffer();
	hex_print_array16((u8*)__sysconf_txt_buffer, 0x101);
	if (was)
		__SYSCONF_DecryptEncryptTextBuffer();
}


void SYSCONF_PrintAllSettings(void){
	if(!__sysconf_inited) return;
	u16 i, count;
	u16 *offset;
	char temp[33], typestring[20];
	u8 nlen;
	offset = (u16*)&__sysconf_buffer[6];
	count = *((u16*)(&__sysconf_buffer[4]));
	printf("Total: %u settings.\n", count);
	for (i = 0; i < count; i++) {
		nlen = (__sysconf_buffer[*offset] & 0x0F)+1;
		memcpy(temp, &__sysconf_buffer[(*offset)+1], nlen);
		temp[nlen] = 0;
		switch(__sysconf_buffer[*offset] >> 5){
			case 1:
				sprintf(typestring, "BIGARRAY[0x%X]", *((u16*)&__sysconf_buffer[(*offset)+nlen+1]) + 1);
			break;
			case 2:
				sprintf(typestring, "SMALLARRAY[0x%X]", __sysconf_buffer[(*offset)+nlen+1] + 1);
			break;
			case 3:
				strcpy(typestring, "BYTE");
			break;
			case 4:
				strcpy(typestring, "SHORT");
			break;
			case 5:
				strcpy(typestring, "LONG");
			break;
			case 7:
				strcpy(typestring, "BOOL");
			break;
			default:
				sprintf(typestring, "Unknown %u", __sysconf_buffer[*offset] >> 5);
		}			
		printf("%3u. 0x%04X: %-10s Type: %s\n", i+1, *offset, temp, typestring);
		offset++;
	}
	
}
#endif /* DEBUG_SYSCONF */

s32 SYSCONF_Init(void)
{
	int fd;
	int ret;
	
	if(__sysconf_inited) return 0;
	
	fd = IOS_Open(__sysconf_file,1);
	if(fd < 0) return fd;
	
	memset(__sysconf_buffer,0,0x4000);
	memset(__sysconf_txt_buffer,0,0x101);
	
	ret = IOS_Read(fd, __sysconf_buffer, 0x4000);
	IOS_Close(fd);
	if(ret != 0x4000) return SYSCONF_EBADFILE;
	
	fd = IOS_Open(__sysconf_txt_file,1);
	if(fd < 0) return fd;
	
	ret = IOS_Read(fd, __sysconf_txt_buffer, 0x100);
	IOS_Close(fd);
	if(ret != 0x100) return SYSCONF_EBADFILE;
	
	if(memcmp(__sysconf_buffer, "SCv0", 4)) return SYSCONF_EBADFILE;
	
	__SYSCONF_DecryptEncryptTextBuffer();
	
	__sysconf_inited = 1;
	return 0;
}

int __SYSCONF_WriteTxtBuffer(void)
{
	u64 tid;
	int ret, fd;
	
	if(!__sysconf_inited) return SYSCONF_ENOTINIT;
	
	if (!__sysconf_txt_buffer_updated) return 0;
		
	ret = ES_GetTitleID(&tid);
	if (ret < 0) return ret;
	
	if (tid != 0x100000002LL) return SYSCONF_EPERMS;
	
	if (__sysconf_buffer_txt_decrypted)
		__SYSCONF_DecryptEncryptTextBuffer();
	
	ret = ISFS_SetAttr(__sysconf_txt_file, 0x1000, 1, 0, 3, 3, 3);
	if (ret < 0) return ret;
	
	fd = IOS_Open(__sysconf_txt_file, 2);
	if(fd < 0) return fd;
		
	ret = IOS_Write(fd, __sysconf_txt_buffer, 0x100);
	IOS_Close(fd);
	if(ret != 0x100) return SYSCONF_EBADWRITE;
		
	ret = ISFS_SetAttr(__sysconf_txt_file, 0x1000, 1, 0, 1, 1, 1);
	if (ret < 0) return ret;
		
	__sysconf_buffer_updated = 0;
		
	return 0;
	
}

int __SYSCONF_WriteBuffer(void)
{
	int ret, fd;
	
	if (!__sysconf_inited) return SYSCONF_ENOTINIT;
		
	if (!__sysconf_buffer_updated) return 0;
		
	fd = IOS_Open(__sysconf_file,2);
	if(fd < 0) return fd;
	
	
	ret = IOS_Write(fd, __sysconf_buffer, 0x4000);
	IOS_Close(fd);
	if(ret != 0x4000) return SYSCONF_EBADFILE;
	
	__sysconf_buffer_updated = 0;
	return 0;
}

s32 SYSCONF_SaveChanges(void)
{
	s32 ret;
	if (!__sysconf_inited) return SYSCONF_ENOTINIT;
	ret = __SYSCONF_WriteBuffer();
	if (ret < 0)
		return ret;
	
	ret = __SYSCONF_WriteTxtBuffer();
	if (ret < 0)
		return ret;
	
	return SYSCONF_ERR_OK;
}

int __SYSCONF_ShiftTxt(char *start, s32 delta)
{
	char *end;
	char temp[0x100];
	
	if (!__sysconf_buffer_txt_decrypted)
		__SYSCONF_DecryptEncryptTextBuffer();
	
	end = strchr((char*)__sysconf_txt_buffer, 0);
	if (end == NULL || end > __sysconf_txt_buffer+0x100) return SYSCONF_EBADFILE;
		
	if (start < __sysconf_txt_buffer || start >= end)
		return SYSCONF_EBADVALUE;
	
	memcpy(temp, start, end-start);
	memcpy(start+delta, temp, end-start);
	//free (temp);
	*(end+delta) = 0;
	return 0;
	
}

int __SYSCONF_GetTxt(const char *name, char *buf, int length)
{
	char *line = __sysconf_txt_buffer;
	char *delim, *end;
	int slen;
	int nlen = strlen(name);
	
	if(!__sysconf_inited) return SYSCONF_ENOTINIT;
		
	if (!__sysconf_buffer_txt_decrypted)
		__SYSCONF_DecryptEncryptTextBuffer();
	
	while(line < (__sysconf_txt_buffer+0x100) ) {
		delim = strchr(line, '=');
		if(delim && ((delim - line) == nlen) && !memcmp(name, line, nlen)) {
			delim++;
			end = strchr(line, '\r');
			if(end) {
				slen = end - delim;
				if(slen < length) {
					memcpy(buf, delim, slen);
					buf[slen] = 0;
					return slen;
				} else {
					//printf("Should be %u is %u", slen, length);
					return SYSCONF_ETOOBIG;
				}
			}
		}
		
		// skip to line end
		while(line < (__sysconf_txt_buffer+0x100) && *line++ != '\n');
	}
	return SYSCONF_ENOENT;
}

/* This function should NOT be used, and was only added for emergency recovery at one point */
int __SYSCONF_AddTxt(const char *name, const char *value)
{
	char *newline;
	char *temp;
	char endline[10];
	u32 length;
	
	if(!__sysconf_inited) return SYSCONF_ENOTINIT;
		
	newline = strchr((char*)__sysconf_txt_buffer, 0);
	if (newline == NULL || newline > __sysconf_txt_buffer+0x100) return SYSCONF_EBADFILE;
		
	
	newline--;
	while (*--newline == '\r');
	newline++;
	
	strcpy(endline, newline);
	
	newline += strlen(endline);
	
	length = strlen(name)+strlen(value)+strlen(endline)+1;
	
	if (newline+length < __sysconf_txt_buffer+0x100){
		temp = malloc(length+1);
		sprintf(temp, "%s=%s%s", name, value, endline);
		strcpy(newline, temp);
	} else {
		printf("Not worth it!");
		return SYSCONF_EBADFILE;
	}
	
	return 0;
}

int __SYSCONF_SetTxt(const char *name, const char *value)
{
	char *line = __sysconf_txt_buffer;
	char *delim, *end;
	int slen;
	int nlen = strlen(name);
	int vlen = strlen(value);
	
	if(!__sysconf_inited) return SYSCONF_ENOTINIT;
		
	if (!__sysconf_buffer_txt_decrypted)
		__SYSCONF_DecryptEncryptTextBuffer();
	
	while(line < (__sysconf_txt_buffer+0x100) ) {
		delim = strchr(line, '=');
		if(delim && ((delim - line) == nlen) && !memcmp(name, line, nlen)) {
			delim++;
			end = strchr(line, '\r');
			if(end) {
				slen = end - delim;
				if(slen == vlen) {
					//printf("vlen: %u slen: %u\n", vlen, slen);
					memcpy(delim, value, vlen);
					__sysconf_txt_buffer_updated = 1;
					return 0;
				} else if(vlen && (vlen < slen || 
					(strchr(end, '\n') + (vlen-slen)) < __sysconf_txt_buffer+0x100)){
					//printf("vlen: %u slen: %u\n", vlen, slen);
					if (__SYSCONF_ShiftTxt(end, vlen-slen)) return -1;
					memcpy(delim, value, vlen);
					__sysconf_txt_buffer_updated = 1;
					return 0;
				} else {
					return SYSCONF_EBADVALUE;
				}
			}
		}
		
		// skip to line end
		while(line < (__sysconf_txt_buffer+0x100) && *line++ != '\n');
	}
	
	return SYSCONF_ENOENT;
}



u8 *__SYSCONF_Find(const char *name)
{
	u16 count;
	u16 *offset;
	int nlen = strlen(name);
	count = *((u16*)(&__sysconf_buffer[4]));
	offset = (u16*)&__sysconf_buffer[6];
	
	while(count--) {
		if((nlen == ((__sysconf_buffer[*offset]&0x0F)+1)) && !memcmp(name, &__sysconf_buffer[*offset+1], nlen))
			return &__sysconf_buffer[*offset];
		offset++;
	}
	return NULL;
}

s32 SYSCONF_GetLength(const char *name)
{
	u8 *entry;

	if(!__sysconf_inited) return SYSCONF_ENOTINIT;
	
	entry = __SYSCONF_Find(name);
	if(!entry) return SYSCONF_ENOENT;
	
	switch(*entry>>5) {
		case 1:
			return *((u16*)&entry[strlen(name)+1]) + 1;
		case 2:
			return entry[strlen(name)+1] + 1;
		case 3:
			return 1;
		case 4:
			return 2;
		case 5:
			return 4;
		case 7:
			return 1;
		default:
			return SYSCONF_ENOTIMPL;	
	}
}

int SYSCONF_GetType(const char *name) 
{
	u8 *entry;
	if(!__sysconf_inited) return SYSCONF_ENOTINIT;
	
	entry = __SYSCONF_Find(name);
	if(!entry) return SYSCONF_ENOENT;
	
	return *entry>>5;
}

s32 SYSCONF_Get(const char *name, void *buffer, u32 length)
{
	u8 *entry;
	s32 len;
	if(!__sysconf_inited) return SYSCONF_ENOTINIT;
	
	entry = __SYSCONF_Find(name);
	if(!entry) return SYSCONF_ENOENT;
	
	len = SYSCONF_GetLength(name);
	if(len<0) return len;
	if(len>length) return SYSCONF_ETOOBIG;
	
	switch(*entry>>5) {
		case SYSCONF_BIGARRAY:
			memcpy(buffer, &entry[strlen(name)+3], len);
			break;
		case SYSCONF_SMALLARRAY:
			memcpy(buffer, &entry[strlen(name)+2], len);
			break;
		case SYSCONF_BYTE:
		case SYSCONF_SHORT:
		case SYSCONF_LONG:
		case SYSCONF_BOOL:
			memset(buffer, 0, length);
			memcpy(buffer, &entry[strlen(name)+1], len);
			break;
		default:
			return SYSCONF_ENOTIMPL;
	}
	return len;
}

s32 SYSCONF_Set(const char *name, const void *value, u32 length)
{
	u8 *entry;
	s32 len;
	if(!__sysconf_inited) return SYSCONF_ENOTINIT;
	
	entry = __SYSCONF_Find(name);
	if(!entry) return SYSCONF_ENOENT;
	
	len = SYSCONF_GetLength(name);
	if(len<0) return len;
	if(length!=len) return SYSCONF_EBADVALUE;
	
	switch(*entry>>5) {
		case SYSCONF_BIGARRAY:
			memcpy(&entry[strlen(name)+3], value, len);
			break;
		case SYSCONF_SMALLARRAY:
			memcpy(&entry[strlen(name)+2], value, len);
			break;
		case SYSCONF_BYTE:
		case SYSCONF_SHORT:
		case SYSCONF_LONG:
		case SYSCONF_BOOL:
			//memset(buffer, 0, length);
			memcpy(&entry[strlen(name)+1], value, len);
			break;
		default:
			return SYSCONF_ENOTIMPL;
	}
	__sysconf_buffer_updated = 1;
	return 0;
}

s32 SYSCONF_GetShutdownMode(void) 
{
	u8 idlesysconf[2] = {0,0};
	int res;
	
	res = SYSCONF_Get("IPL.IDL", idlesysconf, 2);
	if(res<0) return res;
	if(res!=2) return SYSCONF_EBADVALUE;
	return idlesysconf[0];
}

s32 SYSCONF_GetIdleLedMode(void) 
{
	int res;
	u8 idlesysconf[2] = {0,0};
	res = SYSCONF_Get("IPL.IDL", idlesysconf, 2);
	if(res<0) return res;
	if(res!=2) return SYSCONF_EBADVALUE;
	return idlesysconf[1];
}

s32 SYSCONF_GetProgressiveScan(void) 
{
	int res;
	u8 val = 0;
	res = SYSCONF_Get("IPL.PGS", &val, 1);
	if(res<0) return res;
	if(res!=1) return SYSCONF_EBADVALUE;
	return val;
}

s32 SYSCONF_GetEuRGB60(void) 
{
	int res;
	u8 val = 0;
	res = SYSCONF_Get("IPL.E60", &val, 1);
	if(res<0) return res;
	if(res!=1) return SYSCONF_EBADVALUE;
	return val;
}

s32 SYSCONF_GetIRSensitivity(void) 
{
	int res;
	u32 val = 0;
	res = SYSCONF_Get("BT.SENS", &val, 4);
	if(res<0) return res;
	if(res!=4) return SYSCONF_EBADVALUE;
	return val;
}

s32 SYSCONF_GetSensorBarPosition(void) 
{
	int res;
	u8 val = 0;
	res = SYSCONF_Get("BT.BAR", &val, 1);
	if(res<0) return res;
	if(res!=1) return SYSCONF_EBADVALUE;
	return val;
}

s32 SYSCONF_GetPadSpeakerVolume(void) 
{
	int res;
	u8 val = 0;
	res = SYSCONF_Get("BT.SPKV", &val, 1);
	if(res<0) return res;
	if(res!=1) return SYSCONF_EBADVALUE;
	return val;
}

s32 SYSCONF_GetPadMotorMode(void) 
{
	int res;
	u8 val = 0;
	res = SYSCONF_Get("BT.MOT", &val, 1);
	if(res<0) return res;
	if(res!=1) return SYSCONF_EBADVALUE;
	return val;
}

s32 SYSCONF_GetSoundMode(void) 
{
	int res;
	u8 val = 0;
	res = SYSCONF_Get("IPL.SND", &val, 1);
	if(res<0) return res;
	if(res!=1) return SYSCONF_EBADVALUE;
	return val;
}

s32 SYSCONF_GetLanguage(void) 
{
	int res;
	u8 val = 0;
	res = SYSCONF_Get("IPL.LNG", &val, 1);
	if(res<0) return res;
	if(res!=1) return SYSCONF_EBADVALUE;
	return val;
}

s32 SYSCONF_GetCounterBias(u32 *bias) 
{
	int res;
	res = SYSCONF_Get("IPL.CB", bias, 4);
	if(res<0) return res;
	if(res!=4) return SYSCONF_EBADVALUE;
	return SYSCONF_ERR_OK;
}

s32 SYSCONF_GetScreenSaverMode(void) 
{
	int res;
	u8 val = 0;
	res = SYSCONF_Get("IPL.SSV", &val, 1);
	if(res<0) return res;
	if(res!=1) return SYSCONF_EBADVALUE;
	return val;
}

s32 SYSCONF_GetDisplayOffsetH(s8 *offset) 
{
	int res;
	res = SYSCONF_Get("IPL.DH", offset, 1);
	if(res<0) return res;
	if(res!=1) return SYSCONF_EBADVALUE;
	return 0;
}

s32 SYSCONF_GetPadDevices(sysconf_pad_device *devs, int count) 
{
	int res;
	u8 buf[0x461];
	
	res = SYSCONF_Get("BT.DINF", buf, 0x461);
	if(res < 0) return res;
	if((res < 1) || (buf[0] > 0x10)) return SYSCONF_EBADVALUE;

	if(count && devs) {
		memset(devs,0,count*sizeof(sysconf_pad_device));
		if(count < buf[0])
			memcpy(devs,&buf[1],count*sizeof(sysconf_pad_device));
		else
			memcpy(devs,&buf[1],buf[0]*sizeof(sysconf_pad_device));
	}

	res = buf[0];
	return res;
}

s32 SYSCONF_GetNickName(u8 *nickname)
{
	int i, res;
	u16 buf[11];

        res = SYSCONF_Get("IPL.NIK", buf, 0x16);
        if(res < 0) return res;
        if((res != 0x16) || (!buf[0])) return SYSCONF_EBADVALUE;

	for(i=0; i<10; i++)
		nickname[i] = buf[i];
	nickname[10] = 0;

	return res;
}

s32 SYSCONF_GetAspectRatio(void)
{
	int res;
	u8 val = 0;

	res = SYSCONF_Get("IPL.AR", &val, 1);
	if(res < 0) return res;
	if(res!=1) return SYSCONF_EBADVALUE;
	return val;
}

s32 SYSCONF_GetEULA(void)
{
	int res;
	u8 val = 0;

	res = SYSCONF_Get("IPL.EULA", &val, 1);
	if(res < 0) return res;
	if(res!=1) return SYSCONF_EBADVALUE;
	return val;
}

s32 SYSCONF_GetParentalPassword(s8 *password)
{
	int res;
	u8 buf[0x4A];

	res = SYSCONF_Get("IPL.PC", buf, 0x4A);
	if(res < 0) return res;
	if(res!=1) return SYSCONF_EBADVALUE;

	memcpy(password, buf+3, 4);
	password[4] = 0;

	return res;
}

s32 SYSCONF_GetParentalAnswer(s8 *answer)
{
	int res;
	u8 buf[0x4A];

	res = SYSCONF_Get("IPL.PC", buf, 0x4A);
	if(res < 0) return res;
	if(res!=1) return SYSCONF_EBADVALUE;

	memcpy(answer, buf+8, 32);
	answer[32] = 0;

	return res;
}

s32 SYSCONF_GetWiiConnect24(void)
{
	int res;
	u32 val = 0;

	res = SYSCONF_Get("NET.WCFG", &val, 4);
	if(res < 0) return res;
	if(res!=4) return SYSCONF_EBADVALUE;
	return val;
}

s32 SYSCONF_GetRegion(void)
{
	int res;
	char buf[3];

	res = __SYSCONF_GetTxt("GAME", buf, 3);
	if(res < 0) return res;
	if(!strcmp(buf, "JP")) return SYSCONF_REGION_JP;
	if(!strcmp(buf, "US")) return SYSCONF_REGION_US;
	if(!strcmp(buf, "EU")) return SYSCONF_REGION_EU;
	return SYSCONF_EBADVALUE;
}

s32 SYSCONF_GetArea(void)
{
	int res;
	char buf[4];

	res = __SYSCONF_GetTxt("AREA", buf, 4);
	if(res < 0) return res;
	if(!strcmp(buf, "JPN")) return SYSCONF_AREA_JPN;
	if(!strcmp(buf, "USA")) return SYSCONF_AREA_USA;
	if(!strcmp(buf, "EUR")) return SYSCONF_AREA_EUR;
	if(!strcmp(buf, "AUS")) return SYSCONF_AREA_AUS;
	if(!strcmp(buf, "BRA")) return SYSCONF_AREA_BRA;
	if(!strcmp(buf, "TWN")) return SYSCONF_AREA_TWN;
	if(!strcmp(buf, "ROC")) return SYSCONF_AREA_ROC;
	if(!strcmp(buf, "KOR")) return SYSCONF_AREA_KOR;
	if(!strcmp(buf, "HKG")) return SYSCONF_AREA_HKG;
	if(!strcmp(buf, "ASI")) return SYSCONF_AREA_ASI;
	if(!strcmp(buf, "LTN")) return SYSCONF_AREA_LTN;
	if(!strcmp(buf, "SAF")) return SYSCONF_AREA_SAF;
	return SYSCONF_EBADVALUE;
}

s32 SYSCONF_GetVideo(void)
{
	int res;
	char buf[5];

	res = __SYSCONF_GetTxt("VIDEO", buf, 5);
	if(res < 0) return res;
	if(!strcmp(buf, "NTSC")) return SYSCONF_VIDEO_NTSC;
	if(!strcmp(buf, "PAL")) return SYSCONF_VIDEO_PAL;
	if(!strcmp(buf, "MPAL")) return SYSCONF_VIDEO_MPAL;
	return SYSCONF_EBADVALUE;
}

s32 SYSCONF_SetShutdownMode(u8 value) 
{
	u8 idlesysconf[2] = {0,0};
	int res;
	res = SYSCONF_Get("IPL.IDL", idlesysconf, 2);
	if(res<0) return res;
	if(res!=2) return SYSCONF_EBADVALUE;
	
	idlesysconf[0] = value;
	
	return SYSCONF_Set("IPL.IDL", idlesysconf, 2);
}

s32 SYSCONF_SetIdleLedMode(u8 value) 
{
	u8 idlesysconf[2] = {0,0};
	int res;
	res = SYSCONF_Get("IPL.IDL", idlesysconf, 2);
	if(res<0) return res;
	if(res!=2) return SYSCONF_EBADVALUE;
	
	idlesysconf[1] = value;
	
	return SYSCONF_Set("IPL.IDL", idlesysconf, 2);
}


s32 SYSCONF_SetProgressiveScan(u8 value) 
{
	return SYSCONF_Set("IPL.PGS", &value, 1);
}

s32 SYSCONF_SetEuRGB60(u8 value) 
{
	return SYSCONF_Set("IPL.E60", &value, 1);
}


s32 SYSCONF_SetIRSensitivity(u32 value) 
{
	return SYSCONF_Set("BT.SENS", &value, 4);
}

s32 SYSCONF_SetSensorBarPosition(u8 value) 
{
	return SYSCONF_Set("BT.BAR", &value, 1);
}

s32 SYSCONF_SetPadSpeakerVolume(u8 value) 
{
	return SYSCONF_Set("BT.SPKV", &value, 1);
}

s32 SYSCONF_SetPadMotorMode(u8 value) 
{
	return SYSCONF_Set("BT.MOT", &value, 1);
}

s32 SYSCONF_SetSoundMode(u8 value) 
{
	return SYSCONF_Set("IPL.SND", &value, 1);
}

s32 SYSCONF_SetLanguage(u8 value) 
{
	return SYSCONF_Set("IPL.LNG", &value, 1);
}

s32 SYSCONF_SetCounterBias(u32 bias) 
{
	return  SYSCONF_Set("IPL.CB", &bias, 4);
}

s32 SYSCONF_SetScreenSaverMode(u8 value) 
{
	return SYSCONF_Set("IPL.SSV", &value, 1);
}

s32 SYSCONF_SetDisplayOffsetH(s8 offset) 
{
	return SYSCONF_Set("IPL.DH", &offset, 1);
}

s32 SYSCONF_SetPadDevices(const sysconf_pad_device *devs, u8 count) 
{
	u8 buf[0x461] = {0};
	
	if(count > 0x10) return SYSCONF_EBADVALUE;
	buf[0] = count;
	
	if(devs)
		memcpy(&buf[1],devs,count*sizeof(sysconf_pad_device));
	
	return SYSCONF_Set("BT.DINF", buf, 0x461);
}

s32 SYSCONF_SetNickName(const u8 *nickname, u16 length)
{
	int i;
	u16 buf[11] = {0};
	if (length >10) return SYSCONF_EBADVALUE;
		
	for(i=0; i<length; i++)
		buf[i] = nickname[i];
	buf[10] = length;

     return SYSCONF_Set("IPL.NIK", buf, 0x16);
}

s32 SYSCONF_SetAspectRatio(u8 value)
{
	return SYSCONF_Set("IPL.AR", &value, 1);
}

s32 SYSCONF_SetEULA(u8 value)
{
	if (value > 1) return SYSCONF_EBADVALUE;
	return SYSCONF_Set("IPL.EULA", &value, 1);
}

s32 SYSCONF_SetParentalPassword(const s8 *password, u32 length)
{
	int res;
	u8 buf[0x4A] = {0};
	if (length != 4) return SYSCONF_EBADVALUE;
	
	res = SYSCONF_Get("IPL.PC", buf, 0x4A);
	if(res < 0) return res;
	if(res!=1) return SYSCONF_EBADVALUE;

	memcpy(buf+3, password, 4);

	return SYSCONF_Set("IPL.PC", buf, 0x4A);
}

s32 SYSCONF_SetParentalAnswer(const s8 *answer, u32 length)
{
	int res;
	u8 buf[0x4A] = {0};
	if (length != 32) return SYSCONF_EBADVALUE;

	res = SYSCONF_Get("IPL.PC", buf, 0x4A);
	if(res < 0) return res;
	if(res!=1) return SYSCONF_EBADVALUE;

	memcpy(buf+8, answer, length);
	
	return SYSCONF_Set("IPL.PC", buf, 0x4A);
}

s32 SYSCONF_SetWiiConnect24(u32 value)
{
	return SYSCONF_Set("NET.WCFG", &value, 4);
}

s32 SYSCONF_SetRegion(s32 value)
{
	switch(value){
		case SYSCONF_REGION_JP:
			return __SYSCONF_SetTxt("GAME", "JP");
		case SYSCONF_REGION_US:
			return __SYSCONF_SetTxt("GAME", "US");
		case SYSCONF_REGION_EU:
			return __SYSCONF_SetTxt("GAME", "EU");
		default:
			return SYSCONF_EBADVALUE;
	}
}

s32 SYSCONF_SetArea(s32 value)
{
	switch(value){
		case SYSCONF_AREA_JPN:
			return __SYSCONF_SetTxt("AREA", "JPN");
		break;
		case SYSCONF_AREA_USA:
			return __SYSCONF_SetTxt("AREA", "USA");
		break;
		case SYSCONF_AREA_EUR:
			return __SYSCONF_SetTxt("AREA", "EUR");
		break;
		case SYSCONF_AREA_AUS:
			return __SYSCONF_SetTxt("AREA", "AUS");
		break;
		case SYSCONF_AREA_BRA:
			return __SYSCONF_SetTxt("AREA", "BRA");
		break;
		case SYSCONF_AREA_TWN:
			return __SYSCONF_SetTxt("AREA", "TWN");
		break;
		case SYSCONF_AREA_ROC:
			return __SYSCONF_SetTxt("AREA", "ROC");
		break;
		case SYSCONF_AREA_KOR:
			return __SYSCONF_SetTxt("AREA", "KOR");
		break;
		case SYSCONF_AREA_HKG:
			return __SYSCONF_SetTxt("AREA", "HKG");
		break;
		case SYSCONF_AREA_ASI:
			return __SYSCONF_SetTxt("AREA", "ASI");
		break;
		case SYSCONF_AREA_LTN:
			return __SYSCONF_SetTxt("AREA", "LTN");
		break;
		case SYSCONF_AREA_SAF:
			return __SYSCONF_SetTxt("AREA", "SAF");
		break;
		default:
			return SYSCONF_EBADVALUE;
	}
}

s32 SYSCONF_SetVideo(s32 value)
{
	switch(value){
		case SYSCONF_VIDEO_NTSC:
			return __SYSCONF_SetTxt("VIDEO", "NTSC");
		break;
		case SYSCONF_VIDEO_PAL:
			return __SYSCONF_SetTxt("VIDEO", "PAL");
		break;
		case SYSCONF_VIDEO_MPAL:
			return __SYSCONF_SetTxt("VIDEO", "MPAL");
		break;
		default:
			return SYSCONF_EBADVALUE;
	}
}

#endif
