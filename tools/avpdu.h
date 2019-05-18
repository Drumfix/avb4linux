/*
 * avpdu.h
 *
 *  Created on: 14.10.2018
 *      Author: amd64
 */

#include <stdint.h>

#ifndef AVPDU_H_
#define AVPDU_H_

#define ETH_ALEN 6
#define CIP_HEADER_LEN 8
#define SYT_LEN 2
#define STREAM_ID_LEN 8
#define BYTES_PER_SAMPLE 4

/* AVPDU structure with the VLAN tag */

typedef struct {
	uint8_t dest[ETH_ALEN];
	uint8_t src[ETH_ALEN];

#define VLAN_TAG 0x8100

    uint16_t vlan_tag;

#define AVTP_VLAN_TAG 0x600222f0

	uint32_t avtp_vlan_tag;

#define AVTP_IEC61883 0x00;

	uint8_t avtp_subtype;

#define STREAM_ID_VALID     0x80
#define MEDIA_CLOCK_RESTART 0x08
#define GATEWAY_INFO_VALID  0x02
#define TIMESTAMP_VALID     0x01

	uint8_t flags1;

	uint8_t sequence_number;

#define TIMESTAMP_UNCERTAIN 0x01

	uint8_t flags2;

	uint8_t stream_id[STREAM_ID_LEN];
	uint32_t timestamp;
	uint32_t gateway_info;

	uint16_t stream_data_length;

#define CIP_0 0x5f

	uint8_t cip_0;

#define CIP_1 0xa0

	uint8_t cip_1;

#define CIP_2 0x3f

	uint8_t cip_2;

	uint8_t data_block_size;

#define CIP_4 0x00

	uint8_t cip_4;

	uint8_t data_block_continuity;

#define CIP_6 0x90

	uint8_t cip_6;

#define FDF_SAMPLE_RATE_32000  0x00;
#define FDF_SAMPLE_RATE_44100  0x01;
#define FDF_SAMPLE_RATE_48000  0x02;
#define FDF_SAMPLE_RATE_88200  0x03;
#define FDF_SAMPLE_RATE_96000  0x04;
#define FDF_SAMPLE_RATE_176400 0x05;
#define FDF_SAMPLE_RATE_192000 0x06;

    uint8_t fdf;

#define SYT_SAMPLE_RATE_32000  0x08;
#define SYT_SAMPLE_RATE_44100  0x08;
#define SYT_SAMPLE_RATE_48000  0x08;
#define SYT_SAMPLE_RATE_88200  0x10;
#define SYT_SAMPLE_RATE_96000  0x10;
#define SYT_SAMPLE_RATE_176400 0x20;
#define SYT_SAMPLE_RATE_192000 0x20;

	uint16_t syt;

//	uint32_t audio_data[4*8*6*4];

} AVPDU_HEADER;

#pragma PACK(AVPDU_HEADER)

void init_avpdu_header
       (AVPDU_HEADER* p,
        uint8_t *dest,
        uint8_t *src,
        uint8_t *stream_id,
        uint8_t channels,
		uint32_t samplerate);

/* AVPDU structure without the VLAN tag */

typedef struct {

	uint8_t dest[ETH_ALEN];
	uint8_t src[ETH_ALEN];

	uint8_t avtp_subtype;

	uint8_t flags1;

	uint8_t sequence_number;

	uint8_t flags2;

	uint8_t stream_id[STREAM_ID_LEN];
	uint32_t timestamp;
	uint32_t gateway_info;

	uint16_t stream_data_length;

	uint8_t cip_0;

	uint8_t cip_1;

	uint8_t cip_2;

	uint8_t data_block_size;

	uint8_t cip_4;

	uint8_t data_block_continuity;

	uint8_t cip_6;

    uint8_t fdf;

	uint16_t syt;

//	uint32_t audio_data[4*8*6*4];

} AVPDU_HEADER_NO_VLAN;

void init_avpdu_header_no_vlan
       (AVPDU_HEADER_NO_VLAN* p,
        uint8_t *dest,
        uint8_t *src,
        uint8_t *stream_id,
        uint8_t channels,
	uint32_t samplerate);

#pragma PACK(AVPDU_HEADER_NO_VLAN)

typedef struct {

	uint8_t  dest[ETH_ALEN];
	uint8_t  src[ETH_ALEN];
	uint16_t protocol;
	uint8_t  subtype;
	uint8_t  cmd;
        uint16_t status_len;
        uint64_t stream_id;

} AVTP_ACMP_CONTROL_HEADER;

typedef struct {

	uint8_t  dest[ETH_ALEN];
	uint8_t  src[ETH_ALEN];
	uint16_t protocol;
	uint8_t  subtype;
	uint8_t  cmd;
        uint16_t status_len;
        uint64_t entity_id;

} AVTP_AECP_CONTROL_HEADER;

#endif /* AVPDU_H_ */
