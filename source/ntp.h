/* Stolen from https://github.com/lettier/ntpclient */

#ifndef _NTP_
#define _NTP_

#include <unistd.h>

// NTP Epoch 1900-01-01 00:00
// Unix Epoch 1970-01-01 00:00
// Gamecube Epoch 2000-01-01 00:00
#define NTP_TO_UNIX_EPOCH_DELTA 2208988800ull
#define UNIX_EPOCH_TO_GC_EPOCH_DELTA 946684800ull
#define NTP_TO_GC_EPOCH_DELTA (NTP_TO_UNIX_EPOCH_DELTA + UNIX_EPOCH_TO_GC_EPOCH_DELTA)

#define NTP_PORT 123
#define NTP_HOST "pool.ntp.org"
#define NTP_HOME "/apps/sntp"
#define NTP_FILE "sntp.cfg"

// some definitions required to use wplaat's networking library html.[hc]
#define PROGRAM_NAME		"sntp"
#define MAX_LEN				1024
#define TRACE_FILENAME		NTP_HOME "/sntp.trc"
#define URL_TOKEN			"\"gmtOffset\": "

struct sntp_config {
	int offset;
    bool specified_offset;
	bool autosave;
	char ntp_host[80];
    char tzdb_url[MAX_LEN];
};

#define NTP_PACKET_HEADER(li, vn, mode) (((li << 6) & 0b1100'0000) | ((vn << 3) & 0b0011'1000) | ((mode << 0) & 0b0000'0111))
#define NTP_PACKET_LI(header)   (((header) & 0b1100'0000) >> 6)
#define NTP_PACKET_VN(header)   (((header) & 0b0011'1000) >> 3)
#define NTP_PACKET_MODE(header) (((header) & 0b0000'0111) >> 0)

typedef struct {
    uint32_t seconds;
    uint32_t fraction;
} ntp_timestamp;

typedef struct {
    uint8_t header;
    // uint8_t li: 2;           // li.   Two bits.   Leap indicator.
    // uint8_t vn: 3;           // vn.   Three bits. Version number of the protocol.
    // uint8_t mode: 3;         // mode. Three bits. Client will pick mode 3 for client.

    uint8_t stratum;            // Eight bits. Stratum level of the local clock.
    uint8_t poll;               // Eight bits. Maximum interval between successive messages.
    uint8_t precision;          // Eight bits. Precision of the local clock.

    uint32_t rootDelay;         // 32 bits. Total round trip delay time.
    uint32_t rootDispersion;    // 32 bits. Max error aloud from primary clock source.
    uint32_t refId;             // 32 bits. Reference clock identifier.

    ntp_timestamp refTm;        // Reference time-stamp.
    ntp_timestamp origTm;       // Originate time-stamp.
    ntp_timestamp rxTm;         // Received time-stamp.
    ntp_timestamp txTm;         // Transmit time-stamp (the relevant one).
} ntp_packet;                   // Total: 384 bits or 48 bytes.
#endif
