/*
 * avpdu.h
 *
 *  Created on: 14.10.2018
 *      Author: amd64
 */

#ifndef AVPDU_H_
#define AVPDU_H_

#include "igb.h"

#define ETH_ALEN 6
#define CIP_HEADER_LEN 8
#define SYT_LEN 2
#define STREAM_ID_LEN 8
#define BYTES_PER_SAMPLE 4

/* AVPDU structure with the VLAN tag */

typedef struct __attribute__((packed)) {
	u8 dest[ETH_ALEN];
	u8 src[ETH_ALEN];

#define VLAN_TAG 0x81006002

    	u32 vlan_tag;

#define AVTP_VLAN_TAG 0x22f0

	u16 avtp_vlan_tag;

#define AVTP_IEC61883 0x00;

	u8 avtp_subtype;

#define STREAM_ID_VALID     0x80
#define MEDIA_CLOCK_RESTART 0x08
#define GATEWAY_INFO_VALID  0x02
#define TIMESTAMP_VALID     0x01

	u8 flags1;

	u8 sequence_number;

#define TIMESTAMP_UNCERTAIN 0x01

	u8 flags2;

	u8 stream_id[STREAM_ID_LEN];
	u32 timestamp;
	u32 gateway_info;

	u16 stream_data_length;

#define CIP_0 0x5f

	u8 cip_0;

#define CIP_1 0xa0

	u8 cip_1;

#define CIP_2 0x3f

	u8 cip_2;

	u8 data_block_size;

#define CIP_4 0x00

	u8 cip_4;

	u8 data_block_continuity;

#define CIP_6 0x90

	u8 cip_6;

#define FDF_SAMPLE_RATE_32000  0x00;
#define FDF_SAMPLE_RATE_44100  0x01;
#define FDF_SAMPLE_RATE_48000  0x02;
#define FDF_SAMPLE_RATE_88200  0x03;
#define FDF_SAMPLE_RATE_96000  0x04;
#define FDF_SAMPLE_RATE_176400 0x05;
#define FDF_SAMPLE_RATE_192000 0x06;

    u8 fdf;

#define SYT_SAMPLE_RATE_32000  0x08;
#define SYT_SAMPLE_RATE_44100  0x08;
#define SYT_SAMPLE_RATE_48000  0x08;
#define SYT_SAMPLE_RATE_88200  0x10;
#define SYT_SAMPLE_RATE_96000  0x10;
#define SYT_SAMPLE_RATE_176400 0x20;
#define SYT_SAMPLE_RATE_192000 0x20;

	u16 syt;

	u32 audio_data[8*6*4];

} AVPDU_HEADER;

void init_avpdu_header
       (AVPDU_HEADER* p,
        u8 *dest,
        u8 *src,
        u8 *stream_id,
        u8 channels,
	u32 samplerate);

/* AVPDU structure without the VLAN tag */

typedef struct __attribute__((packed)) {

	u8 dest[ETH_ALEN];
	u8 src[ETH_ALEN];

	u16 avtp_vlan_tag;

	u8 avtp_subtype;

	u8 flags1;

	u8 sequence_number;

	u8 flags2;

	u8 stream_id[STREAM_ID_LEN];
	u32 timestamp;
	u32 gateway_info;

	u16 stream_data_length;

	u8 cip_0;

	u8 cip_1;

	u8 cip_2;

	u8 data_block_size;

	u8 cip_4;

	u8 data_block_continuity;

	u8 cip_6;

    	u8 fdf;

	u16 syt;

	u32 audio_data[8*6*4];

} AVPDU_HEADER_NO_VLAN;

void init_avpdu_header_no_vlan
       (AVPDU_HEADER_NO_VLAN* p,
        u8 *dest,
        u8 *src,
        u8 *stream_id,
        u8 channels,
		u32 samplerate);

#endif /* AVPDU_H_ */
