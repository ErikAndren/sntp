#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <network.h>
#include <errno.h>

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

//static queue_item queue_items[MAX_QUEUE_ITEMS];

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

	printf ("\nNTP client demo\n");
	printf("Configuring network. Continously press home key to abort\n");

	net_deinit();

	ret = SYSCONF_Init();
	if (ret < 0) {
		printf("Failed to init sysconf and settings.txt. Err: %d\n", ret);
		exit(1);
	}

	for (int i = 0; i < NO_RETRIES; i++) {
		WPAD_ScanPads();

	    u32 buttonsDown = WPAD_ButtonsDown(0);

		if (buttonsDown & WPAD_BUTTON_HOME) {
			printf("Home button pressed. Exiting...\n");
			exit(0);
		}

		printf("\rTry: %d", i);
		ret = net_init();
		if ((ret == -EAGAIN) || (ret == -ETIMEDOUT)) {
			usleep(50 * 1000);
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

	printf("Network configured, ip: %s\n", inet_ntoa(hostip));

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
	while (true) {
		VIDEO_WaitVSync();
		WPAD_ScanPads();

		buttonsDown = WPAD_ButtonsDown(0);
		/* if (buttonsDown != 0) { */
		/* 	printf("Buttons: 0x%0x\n", buttonsDown); */
		/* } */

		if (buttonsDown & WPAD_BUTTON_HOME) {
			printf("Home button pressed. Exiting...\n");
			exit(0);
		}

		if (buttonsDown != 0) {
			queue_item *q = malloc(sizeof(queue_item));
			if (q == NULL) {
				printf("Failed to allocate queue item! Err:%d, %s\n", errno, strerror(errno));
				continue;
			}
			q->buttonsDown = buttonsDown;

//			printf("Enqueing queue with item: 0x%x\n", q->buttonsDown);
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

	memset(&packet, 0, sizeof(ntp_packet));

	// Set the first byte's bits to 00,011,011 for li = 0, vn = 3, and mode = 3
	*((char *) &packet + 0) = 0b00011011;

	// Cannot use IPPROTO_UDP, this will return error 121
	sockfd = net_socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

	if (sockfd < 0) {
		printf("Failed to create socket %d:%s. Aborting!\n", sockfd, strerror(sockfd));
		return NULL;
	}

	server = net_gethostbyname(NTP_HOST);
	if (server == NULL) {
		printf("Failed to resolve ntp host %s. Errno: %d, %s. Aborting!\n", NTP_HOST, errno, strerror(errno));
		return NULL;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;

	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

	printf("Resolved %s to %s\n", NTP_HOST, inet_ntoa(serv_addr.sin_addr));

	serv_addr.sin_port = htons(NTP_PORT);

	for (int i = 0; i < 50; i++) {
		n = net_connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
		if (n < 0) {
			printf("Failed to establish connection to NTP server. Err: %d:%s, Aborting!\n", n, strerror(n));
			usleep(50 * 1000);
		} else {
			break;
		}
	}
	if (n < 0) {
		printf("Giving up\n");
		return NULL;
	}

	n = net_write(sockfd, &packet, sizeof(ntp_packet));
	if (n != sizeof(ntp_packet)) {
		printf("Failed to write the full ntp packet. Err: %d:%s. Aborting!\n", n, strerror(n));
		return NULL;
	}

	n = net_read(sockfd, &packet, sizeof(ntp_packet));
	if (n < 0) {
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

	time_t txTm = (time_t) (packet.txTm_s - NTP_TO_UNIX_EPOCH_DELTA);
	ntp_time_in_gc_epoch = packet.txTm_s - NTP_TO_GC_EPOCH_DELTA;

	n = SYSCONF_GetCounterBias(&bias);
	if (n < 0) {
		printf("%s:%d. Failed to get counter bias. Err: %d. Aborting!\n", __FILE__, __LINE__, n);
		return NULL;
	}
	printf("Counter bias is %u\n", bias);

	local_time = rtc_s + bias;
	printf("ntp time: %llu, local time: %llu, diff: %lld\n", ntp_time_in_gc_epoch, local_time, ntp_time_in_gc_epoch - local_time);

	printf("Use left and right button to adjust time zone of time below.\nPress A to write time to system memory\n");

	printf("NTP Time: %s\n", ctime(&txTm));

	timezone = 0;

	// Calculate new bias
	if (ntp_time_in_gc_epoch > local_time) {
		bias += ntp_time_in_gc_epoch - local_time;
	} else {
		bias -= local_time - ntp_time_in_gc_epoch;
	}

	while (true) {
		uint32_t old_rtc_s = 0;

		queue_item *q = (queue_item *) __lwp_queue_get(&queue);

		n = __SYS_GetRTC(&rtc_s);
		if (n == 0) {
			printf("Failed to get RTC. Err: %d. Aborting!\n", n);
			return NULL;
		}

		if (old_rtc_s != rtc_s) {
			old_rtc_s = rtc_s;
			local_time = rtc_s + bias + UNIX_EPOCH_TO_GC_EPOCH_DELTA;

			char s[80];
			struct tm * p = localtime((time_t *) &local_time);
			strftime(s, 80, "%H:%M:%S %A %B %d %Y", p);

			printf("\rNew system time: %s (Timezone: %+03d)", s, timezone);
		}

		if (q == NULL) {
			//FIXME: Replace with thread yield
			usleep(50 * 1000);
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
			printf("Checking written counter bias value\n");
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

			printf("Counter bias successfully updated. You can now terminate this program by pressing the home key or continue to alter the time zones\n");
		}
		free(q);
	}

	/* if (ntp_time_in_gc_epoch > local_time) { */
	/* 	// Need to increase counter bias to sync with ntp time */
	/* 	printf("Increasing bias by %llu\n", ntp_time_in_gc_epoch - local_time); */
	/* 	bias += (ntp_time_in_gc_epoch - local_time); */
	/* } else { */
	/* 	printf("Decreasing bias by %llu\n", local_time - ntp_time_in_gc_epoch); */
	/* 	// Need to decrease counter bias to sync with ntp_time */
	/* 	bias -= (local_time - ntp_time_in_gc_epoch); */
	/* } */
	/* printf("Adjusting bias to %u\n", bias); */

	/* printf("Rereading counter bias\n"); */
	/* n = SYSCONF_GetCounterBias(&bias); */
	/* if (n < 0) { */
	/* 	printf("Failed to get counter bias. Err: %d. Aborting!\n", n); */
	/* 	return NULL; */
	/* } */

	/* local_time = rtc_s + bias; */
	/* printf("ntp time: %llu, local time: %llu, diff: %lld\n", ntp_time_in_gc_epoch, local_time, ntp_time_in_gc_epoch - local_time); */


	return NULL;
}

//---------------------------------------------------------------------------------
void *initialise() {
//---------------------------------------------------------------------------------

	void *framebuffer;

	VIDEO_Init();
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
