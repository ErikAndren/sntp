#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <network.h>
#include <debug.h>
#include <errno.h>
#include <wiiuse/wpad.h>

#include "ntp.h"

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void *initialise();
void *httpd (void *arg);

#define PORT 80

static	lwp_t httd_handle = (lwp_t)NULL;

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------
	s32 ret;

	char localip[16] = {0};
	char gateway[16] = {0};
	char netmask[16] = {0};

	xfb = initialise();

	printf ("\nlibogc network demo\n");
	printf("Configuring network ...\n");

	// Configure the network interface
	ret = if_config ( localip, netmask, gateway, TRUE, 20);
	if (ret >= 0) {
		printf ("network configured, ip: %s, gw: %s, mask %s\n", localip, gateway, netmask);

		LWP_CreateThread(	&httd_handle,	/* thread handle */
							httpd,			/* code */
							localip,		/* arg pointer for thread */
							NULL,			/* stack base */
							16*1024,		/* stack size */
							50				/* thread priority */ );
	} else {
		printf ("network configuration failed!\n");
	}

	while(1) {

		VIDEO_WaitVSync();
		WPAD_ScanPads();

		int buttonsDown = WPAD_ButtonsDown(0);

		if (buttonsDown & WPAD_BUTTON_HOME) {
			exit(0);
		}
	}

	return 0;
}

void *httpd (void *arg) {
	int sockfd, n;

	ntp_packet packet;

	struct sockaddr_in serv_addr;
	struct hostent *server;

	memset(&packet, 0, sizeof(ntp_packet));

	// Set the first byte's bits to 00,011,011 for li = 0, vn = 3, and mode = 3
	*((char *) &packet + 0) = 0b00011011;

	sockfd = net_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == INVALID_SOCKET) {
		printf("Failed to create socket. Aborting!\n");
		return NULL;
	}

	server = net_gethostbyname(NTP_HOST);
	if (server == NULL) {
		printf("Failed to resolve ntp host %s. Aborting!\n", NTP_HOST);
		return NULL;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;

	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

	serv_addr.sin_port = htons(NTP_PORT);

	n = net_connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	if (n < 0) {
		printf("Failed to establish connection to NTP server. Err: %d:%s, Aborting!\n", n, strerror(errno));
		return NULL;
	}

	n = net_write(sockfd, &packet, sizeof(ntp_packet));
	if (n != sizeof(ntp_packet)) {
		printf("Failed to write the full ntp packet. Aborting!\n");
		return NULL;
	}

	n = net_read(sockfd, &packet, sizeof(ntp_packet));
	if (n < 0) {
		if (n < 0) {
			printf("Error while receiving ntp packet. Err: %d:%d:%s. Aborting!\n", n, errno, strerror(errno));
		} else {
			printf("Did not receive full ntp packet. Got %d. Aborting!\n", n);
		}
		return NULL;
	}

	/* Swap seconds and fractions to host byte order */
	packet.txTm_s = ntohl(packet.txTm_s);
	packet.txTm_f = ntohl(packet.txTm_f);

	time_t txTm = (time_t) (packet.txTm_s - NTP_TIMESTAMP_DELTA);

	printf("Time: %s", ctime(&txTm));

	return NULL;
}

//---------------------------------------------------------------------------------
void *initialise() {
//---------------------------------------------------------------------------------

	void *framebuffer;

	VIDEO_Init();
	WPAD_Init();

	rmode = VIDEO_GetPreferredMode(NULL);
	framebuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(framebuffer,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(framebuffer);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	return framebuffer;

}
