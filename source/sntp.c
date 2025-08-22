#include <stdio.h>
#include <stdlib.h>
#include <limits.h> // LONG_MIN and LONG_MAX, for strtol
#include <string.h>
#include <malloc.h>
#include <network.h>
#include <errno.h>
#include <fat.h>

#include <wiiuse/wpad.h>
#include <ogc/lwp_watchdog.h>

#include "ntp.h"
#include "sysconf.h"
#include "http.h"
#include "trace.h"

extern u32 __SYS_GetRTC(u32 *gctime);

void initialise();
int ntp_get_time(uint64_t* gctime, uint64_t* ticks);
void get_tz_offset();

static void *xfb = NULL;
GXRModeObj *rmode = NULL;

static struct sntp_config sntp_config;

u32 get_buttons(void) {
	u32 buttonsDown = 0, buttonsDownGC = 0;

	WPAD_ScanPads();
	PAD_ScanPads();
	buttonsDown = WPAD_ButtonsDown(0);
	buttonsDownGC = PAD_ButtonsDown(0);

	if (buttonsDown & WPAD_BUTTON_HOME || buttonsDownGC & PAD_BUTTON_START || SYS_ResetButtonDown()) {
		printf("\nHome button pressed. Exiting...\n");
		exit(0);
	}
	if (buttonsDownGC & PAD_BUTTON_LEFT) {
		buttonsDown |= WPAD_BUTTON_LEFT;
	}
	if (buttonsDownGC & PAD_BUTTON_RIGHT) {
		buttonsDown |= WPAD_BUTTON_RIGHT;
	}
	if (buttonsDownGC & PAD_BUTTON_A) {
		buttonsDown |= WPAD_BUTTON_A;
	}

	return buttonsDown;
}

static const char* time_string(time_t time) {
	struct tm _tm;
	static char time_str[80];

	strftime(time_str, sizeof(time_str), "%H:%M:%S %B %d %Y", localtime_r(&time, &_tm));
	return time_str;
}

void sntp_parse_config(void) {
	FILE* ntpf = NULL;
	char linebuffer[100];

	chdir(NTP_HOME);
	ntpf = fopen(NTP_FILE, "r");
	if (ntpf == NULL)
		return;

	while (fgets(linebuffer, sizeof(linebuffer), ntpf)) {
		linebuffer[strcspn(linebuffer, "\r\n")] = '\0'; // remove the newline at the end

		if (linebuffer[0] == '#')
			continue;

		char* valueptr = strchr(linebuffer, '=');
		if (valueptr)
			valueptr += 1;

		/* strlen(<string constant>) is automatically optmized :) */
		if (strncasecmp(linebuffer, "offset", strlen("offset")) == 0) {
			if (!valueptr)
				continue;

			char* endptr;
			long offset = strtol(valueptr, &endptr, 10);
			if (offset == LONG_MIN || offset == LONG_MAX || *endptr != '\0') {
				printf("Invalid 'offset' value %s\n", valueptr);
				continue;
			}

			printf("Using UTC offset %+ld\n", offset);
			sntp_config.specified_offset = true;
			sntp_config.offset = offset;
		}

		else if (strncasecmp(linebuffer, "autosave", strlen("autosave")) == 0) {
			printf("Autosave is enabled\n");
			sntp_config.autosave = true;
		}

		else if (strncasecmp(linebuffer, "tzdb_url", strlen("tzdb_url")) == 0) {
			if (!valueptr)
				continue;

			printf("Using timezone url: \n%s\n", valueptr);
			strncpy(sntp_config.tzdb_url, valueptr, sizeof(sntp_config.tzdb_url) - 1);
		}

		else if (strncasecmp(linebuffer, "ntp_host", strlen("ntp_host")) == 0) {
			if (!valueptr)
				continue;

			printf("Using NTP host: %s\n", valueptr);
			strncpy(sntp_config.ntp_host, valueptr, sizeof(sntp_config.ntp_host) - 1);
		}
	}

	fclose(ntpf);
}

#define NO_RETRIES 20

int main(int argc, char **argv) {
	s32 ret;
	struct in_addr hostip;

	uint32_t rtc_s;
	time_t local_time;
	uint64_t utc_time_in_gc_epoch;
	uint64_t start_time = 0;
	u32 bias;

	initialise();

	printf ("\nNTP time synchronizer\n");

	ret = SYSCONF_Init();
	if (ret < 0) {
		printf("Failed to init sysconf and settings.txt. Err: %d\n", ret);
		goto exit;
	}

	sntp_parse_config();

	for (int i = 1; i <= NO_RETRIES; i++) {
		get_buttons();

		printf("\r\e[2K" /* clear line */ "Try: % 2d of configuring network. Hold home key to abort", i);
		fflush(stdout);

		net_deinit();

		ret = net_init();
		if ((ret == -EAGAIN) || (ret == -ETIMEDOUT)) {
			usleep(10 * 1000);
			continue;
		}
		break;
	}
	printf("\n");

	if (ret < 0) {
		printf ("Network configuration failed: \n %s (%d)\n", strerror(-ret), ret);
		goto exit;
	}

	hostip.s_addr = net_gethostip();
	if (hostip.s_addr == 0) {
		printf("Failed to get local ip address.\n");
		goto exit;
	}

	printf("Network configured, local ip: %s\n", inet_ntoa(hostip));

	ret = ntp_get_time(&utc_time_in_gc_epoch, &start_time);
	if (ret < 0)
		goto exit;

	get_tz_offset();
	if (!sntp_config.autosave) {
		printf("Use left and right button to adjust time zone\nPress A to write time to system config\n");

		int offset = sntp_config.offset;


		uint64_t update_time = 0;
		bool loop = true;
		while (loop) {
			uint64_t now = gettime();
			if (diff_sec(update_time, now) >= 1) {
				update_time = now;
				local_time = utc_time_in_gc_epoch + UNIX_EPOCH_TO_GC_EPOCH_DELTA + offset + diff_sec(start_time, now);

				int timezone = offset;
				int timezone_min = abs(offset % 3600 / 60);
				char timezone_min_str[8];

				if(timezone_min != 0)
				{
					snprintf(timezone_min_str, sizeof(timezone_min_str),":%d",timezone_min);
				}
				else
				{
					*timezone_min_str = '\0';
				}
				printf("\r\e[2K" "Proposed NTP system time: %s (Timezone: %+03d%s)", time_string(local_time), timezone, timezone_min_str);
				fflush(stdout);
			}

			for (int i = 0; i < 4; i++)
				VIDEO_WaitVSync();

			u32 buttons = get_buttons();

			if (buttons & WPAD_BUTTON_LEFT) {
				offset -= 1800;
				update_time = 0;

			} else if (buttons & WPAD_BUTTON_RIGHT) {
				offset += 1800;
				update_time = 0;

			} else if (buttons & WPAD_BUTTON_A) {
				loop = false;
			}
		}
	}

	ret = __SYS_GetRTC(&rtc_s);
	if (ret == 0) {
		printf("Failed to get RTC.\n");
		goto exit;
	}

	printf("\nWriting new time (bias) to sysconf\n");

	// Calculate new bias
	bias = (utc_time_in_gc_epoch + diff_sec(start_time, gettime()) + sntp_config.offset) - rtc_s;

	ret = SYSCONF_SetCounterBias(bias);
	if (ret < 0) {
		printf("Failed to set counter bias. Err: %d\n", ret);
		goto exit;
	}

	ret = SYSCONF_SaveChanges();
	if (ret != 0) {
		printf("Failed to save updated counter bias. Err: %d\n", ret);
		goto exit;
	}

	printf("Successfully saved counter bias change\n");

	__SYS_GetRTC(&rtc_s);
	local_time = rtc_s + bias + UNIX_EPOCH_TO_GC_EPOCH_DELTA;
	printf("Time successfully updated to: %s\n", time_string(local_time));
	ret = 0;

exit:
	printf("Exiting in 5 seconds...");
	sleep(5);
	return ret;
}

int ntp_get_time(uint64_t* gctime, uint64_t* ticks) {
	int sockfd, n;
	ntp_packet packet;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	memset(&packet, 0, sizeof(ntp_packet));
	packet.li = 0;
	packet.vn = 3;
	packet.mode = 3; // client

	// Cannot use IPPROTO_UDP, this will return error 121
	sockfd = net_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sockfd < 0) {
		printf("Failed to create socket: \n%s (%d)\n", strerror(-sockfd), sockfd);
		return sockfd;
	}

	const char* ntp_host = sntp_config.ntp_host;
	if (*ntp_host == '\0')
		ntp_host = NTP_HOST;

	server = net_gethostbyname(ntp_host);
	if (server == NULL) {
		int _errno = errno;
		printf("Failed to resolve %s: \n%s (%d)\n", ntp_host, strerror(-_errno), _errno);
		return _errno;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
	serv_addr.sin_port = htons(NTP_PORT);

	printf("Resolved NTP server %s to %s\n\n", ntp_host, inet_ntoa(serv_addr.sin_addr));

	n = net_connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	if (n < 0) {
		printf("Failed to establish connection to NTP server: \n%s (%d)\n", strerror(-n), n);
		return n;
	}

	n = net_write(sockfd, &packet, sizeof(ntp_packet));
	if (n != sizeof(ntp_packet)) {
		if (n < 0) {
			printf("Error while sending NTP packet: \n%s (%d)\n", strerror(-n), n);
			return n;
		} else {
			printf("Failed to send full NTP packet (sent %u bytes, expected %u)\n", n, sizeof(ntp_packet));
			return -1;
		}
	}

	n = net_read(sockfd, &packet, sizeof(ntp_packet));
	if (n != sizeof(ntp_packet)) {
		if (n < 0) {
			printf("Error while recieving NTP packet: \n%s (%d)\n", strerror(-n), n);
			return n;
		} else {
			printf("Did not recieve full NTP packet (sent %u bytes, expected %u)\n", n, sizeof(ntp_packet));
			return -1;
		}
	}

	*ticks = gettime();
	/* Swap seconds to host byte order */
	*gctime = ntohl(packet.txTm_s) - NTP_TO_GC_EPOCH_DELTA;

	n = net_close(sockfd);
	sockfd = -1;
	if (n < 0) {
		printf("Failed to close NTP connection: \n%s (%d)\n", strerror(-n), n);
	}

	return 0;
}

//---------------------------------------------------------------------------------
void initialise() {
//---------------------------------------------------------------------------------

	VIDEO_Init();
	PAD_Init();
	WPAD_Init();
	fatInitDefault();
	/*
	if (WPAD_Init() != WPAD_ERR_NONE) {
		printf("Failed to initialize any wii motes\n"); // can't show up. console is not initialized yet
	}
	*/

	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) {
		VIDEO_WaitVSync();
	}
}

//---------------------------------------------------------------------------------
void get_tz_offset() {
//---------------------------------------------------------------------------------

	char *s_fn="get_tz_offset" ;
	char userData1[MAX_LEN];
	char userData2[MAX_LEN];

	if (*sntp_config.tzdb_url == '\0')
		return;

	// Open trace module
	traceOpen(TRACE_FILENAME);
	traceEvent(s_fn, 0,"%s %s Started", PROGRAM_NAME, PROGRAM_VERSION);

	tcp_start_thread(PROGRAM_NAME, PROGRAM_VERSION,
						"", sntp_config.tzdb_url,
						"", "",
						"", "",
						"", "",
						URL_TOKEN, userData1, userData2);
	printf("Querying online for GMT offset...\n");
	int tcp_state = tcp_get_state_nr();
	for(int retries = 0; retries < 15 && tcp_state != TCP_IDLE; ++retries)
	{
		sleep(1);
		tcp_state = tcp_get_state_nr();
	}
	if(tcp_state == TCP_IDLE)
	{
		sntp_config.offset = atoi(tcp_get_version());
		printf("Found GMT offset online of %d\n", sntp_config.offset);
	}
	else
	{
		printf("GMT offset not found online\n");
		sntp_config.autosave = sntp_config.specified_offset;
	}
	tcp_stop_thread();

	traceEvent(s_fn, 0,"%s %s Stopped", PROGRAM_NAME, PROGRAM_VERSION);
	traceClose();
}
