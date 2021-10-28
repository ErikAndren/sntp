#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <network.h>
#include <errno.h>
#include <fat.h>

#include <wiiuse/wpad.h>
#include <ogc/lwp_queue.h>

#include "ntp.h"
#include "sysconf.h"

extern u32 __SYS_GetRTC(u32 *gctime);

void *initialise();
void *ntp_client(void *arg);

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

static lwp_t ntp_handle = (lwp_t) NULL;

#define MAX_QUEUE_ITEMS 4
static lwp_queue queue;

typedef struct _queue_item {
	lwp_node node;
	u32 buttonsDown;
} queue_item;

static __inline__ lwp_node* __lwp_queue_head(lwp_queue *queue)
{
	return (lwp_node*)queue;
}

static __inline__ lwp_node* __lwp_queue_tail(lwp_queue *queue)
{
	return (lwp_node*)&queue->perm_null;
}

static __inline__ void __lwp_queue_init_empty(lwp_queue *queue)
{
	queue->first = __lwp_queue_tail(queue);
	queue->perm_null = NULL;
	queue->last = __lwp_queue_head(queue);
}

#define NO_RETRIES 20

int main(int argc, char **argv) {
	s32 ret;
	struct in_addr hostip;

	xfb = initialise();

	__lwp_queue_init_empty(&queue);

	printf ("\nNTP time synchronizer\n");

	ret = SYSCONF_Init();
	if (ret < 0) {
		printf("Failed to init sysconf and settings.txt. Err: %d\n", ret);
		exit(1);
	}

	for (int i = 1; i <= NO_RETRIES; i++) {
		WPAD_ScanPads();

	    u32 buttonsDown = WPAD_ButtonsDown(0);

		PAD_ScanPads();
		u32 buttonsDownGC = PAD_ButtonsDown(0);

		if (buttonsDown & WPAD_BUTTON_HOME || buttonsDownGC & PAD_BUTTON_START) {
			printf("\nHome button pressed. Exiting...\n");
			exit(0);
		}

		printf("\rTry: % 2d of configuring network. Hold home key to abort", i);
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
		printf ("Network configuration failed: %d:%s. Aborting!\n", ret, strerror(ret));
		exit(0);
	}

	hostip.s_addr = net_gethostip();
	if (hostip.s_addr == 0) {
		printf("Failed to get configured ip address. Aborting!\n");
		exit(0);
	}

	printf("Network configured, local ip: %s\n", inet_ntoa(hostip));

	if (LWP_CreateThread(&ntp_handle,	/* thread handle */
					 ntp_client,	/* code */
					 NULL,		    /* arg pointer for thread */
					 NULL,			/* stack base */
					 16*1024,		/* stack size */
					 50				/* thread priority */ ) < 0) {
		printf("Failed to create ntp clint thread. Aborting!\n");
		exit(0);
	}

	uint32_t buttonsDown;
	u32 buttonsDownGC;
	while (true) {
		VIDEO_WaitVSync();
		WPAD_ScanPads();

		buttonsDown = WPAD_ButtonsDown(0);
		PAD_ScanPads();
		buttonsDownGC = PAD_ButtonsDown(0);

		if (buttonsDown & WPAD_BUTTON_HOME || buttonsDownGC & PAD_BUTTON_START) {
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
		
		if (buttonsDown != 0) {
			queue_item *q = malloc(sizeof(queue_item));
			if (q == NULL) {
				printf("Failed to allocate queue item! Err:%d, %s\n", errno, strerror(errno));
				continue;
			}
			q->buttonsDown = buttonsDown;

			__lwp_queue_append(&queue, (lwp_node *) q);
		}
	}

	return 0;
}

void *ntp_client(void *arg) {
	int sockfd, n;
	ntp_packet packet;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	uint32_t rtc_s;
	uint64_t local_time;
	uint64_t ntp_time_in_gc_epoch;
	u32 bias, chk_bias;
	s32 timezone;
	char ntp_host[80];
	FILE *ntpf;
	
	// allow overriding default ntp server 
	strcpy(ntp_host, NTP_HOST);
	chdir(NTP_HOME);
	ntpf = fopen(NTP_FILE, "r");
	if(ntpf != NULL) {
		fgets(ntp_host, sizeof(ntp_host), ntpf);
		fclose(ntpf);
		printf("Using NTP server %s from file %s\n", ntp_host, NTP_FILE);
	}

	memset(&packet, 0, sizeof(ntp_packet));

	// Set the first byte's bits to 00,011,011 for li = 0, vn = 3, and mode = 3
	*((char *) &packet + 0) = 0b00011011;

	// Cannot use IPPROTO_UDP, this will return error 121
	sockfd = net_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

	if (sockfd < 0) {
		printf("Failed to create socket %d:%s. Aborting!\n", sockfd, strerror(sockfd));
		return NULL;
	}

	server = net_gethostbyname(ntp_host);
	if (server == NULL) {
		printf("Failed to resolve ntp host %s. Errno: %d, %s. Aborting!\n", ntp_host, errno, strerror(errno));
		return NULL;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;

	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

	printf("Resolved NTP server %s to %s\n\n", ntp_host, inet_ntoa(serv_addr.sin_addr));

	serv_addr.sin_port = htons(NTP_PORT);

	n = net_connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	if (n < 0) {
		printf("Failed to establish connection to NTP server. Err: %d:%s, Aborting!\n", n, strerror(n));
		return NULL;
	}

	n = net_write(sockfd, &packet, sizeof(ntp_packet));
	if (n != sizeof(ntp_packet)) {
		printf("Failed to write the full ntp packet. Err: %d:%s. Aborting!\n", n, strerror(n));
		return NULL;
	}

	n = net_read(sockfd, &packet, sizeof(ntp_packet));
	if (n < sizeof(ntp_packet)) {
		if (n < 0) {
			printf("Error while receiving ntp packet. Err: %d:%s. Aborting!\n", n, strerror(n));
		} else {
			printf("Did not receive full ntp packet. Got %d bytes. Aborting!\n", n);
		}
		return NULL;
	}

	/* Swap seconds to host byte order */
	packet.txTm_s = ntohl(packet.txTm_s);

	n = __SYS_GetRTC(&rtc_s);
	if (n == 0) {
		printf("Failed to get RTC. Err: %d. Aborting!\n", n);
		return NULL;
	}

	ntp_time_in_gc_epoch = packet.txTm_s - NTP_TO_GC_EPOCH_DELTA;

	n = SYSCONF_GetCounterBias(&bias);
	if (n < 0) {
		printf("%s:%d. Failed to get counter bias. Err: %d. Aborting!\n", __FILE__, __LINE__, n);
		return NULL;
	}

	local_time = rtc_s + bias;

	printf("Use left and right button to adjust time zone\nPress A to write time to system config\n");

	timezone = 0;

	// Calculate new bias
	bias = ntp_time_in_gc_epoch - rtc_s;

	uint32_t old_rtc_s = 0;
	char time_str[80];
	struct tm *p;

	while (true) {
		queue_item *q = (queue_item *) __lwp_queue_get(&queue);

		n = __SYS_GetRTC(&rtc_s);
		if (n == 0) {
			printf("Failed to get RTC. Err: %d. Aborting!\n", n);
			return NULL;
		}

		if (old_rtc_s != rtc_s) {
			old_rtc_s = rtc_s;
			local_time = rtc_s + bias + UNIX_EPOCH_TO_GC_EPOCH_DELTA;

			p = localtime((time_t *) &local_time);
			strftime(time_str, sizeof(time_str), "%H:%M:%S %B %d %Y", p);

			printf("\rProposed NTP system time: %s (Timezone: %+03d)   ", time_str, timezone);
			fflush(stdout);
		}

		if (q == NULL) {
			LWP_YieldThread();
			continue;
		}

		if (q->buttonsDown & WPAD_BUTTON_LEFT) {
			if (timezone > -12) {
				timezone--;
				bias -= 3600;
			}

		} else if (q->buttonsDown & WPAD_BUTTON_RIGHT) {
			if (timezone < 12) {
				timezone++;
				bias += 3600;
			}

		} else if (q->buttonsDown & WPAD_BUTTON_A) {
			printf("\nWriting new time (bias) to sysconf\n");
			n = SYSCONF_SetCounterBias(bias);
			if (n < 0) {
				printf("Failed to set counter bias. Err: %d. Aborting!\n", n);
				return NULL;
			}

			n = SYSCONF_SaveChanges();
			if (n != 0) {
				printf("Failed to save updated counter bias. Err: %d\n", n);
			}
			printf("Successfully saved counter bias change\n");

			printf("Checking time written (counter bias) value\n");
			chk_bias = 0;
			n = SYSCONF_GetCounterBias(&chk_bias);
			if (n < 0) {
				printf("Failed to get counter bias. Err: %d. Aborting!\n", n);
				return NULL;
			}

			if (bias != chk_bias) {
				printf("Failed to verify written bias value. Got %u, expected %u\n", chk_bias, bias);
				return NULL;
			}

			local_time = rtc_s + bias + UNIX_EPOCH_TO_GC_EPOCH_DELTA;
			p = localtime((time_t *) &local_time);
			strftime(time_str, sizeof(time_str), "%H:%M:%S %B %d %Y", p);

			printf("Time successfully updated to: %s\n", time_str);
			printf("You may now terminate this program by pressing the home key\n(or continue to adjust the time zones)\n");
		}
		free(q);
	}

	return NULL;
}

//---------------------------------------------------------------------------------
void *initialise() {
//---------------------------------------------------------------------------------

	void *framebuffer;

	VIDEO_Init();
	fatInitDefault();
	PAD_Init();
	if (WPAD_Init() != WPAD_ERR_NONE) {
		printf("Failed to initialize any wii motes\n");
	}

	rmode = VIDEO_GetPreferredMode(NULL);
	framebuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(framebuffer,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(framebuffer);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) {
		VIDEO_WaitVSync();
	}

	return framebuffer;
}
