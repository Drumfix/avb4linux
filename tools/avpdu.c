/*
 * avpdh.c
 *
 *  Created on: 14.10.2018
 *      Author: amd64
 */

#include "avpdu.h"
#include <string.h>
#include <arpa/inet.h>

void init_avpdu_header
       (AVPDU_HEADER* p,
        uint8_t *dest,
        uint8_t *src,
        uint8_t *stream_id,
        uint8_t channels,
		uint32_t samplerate)
{
	memset(p, 0, sizeof(AVPDU_HEADER));

	uint16_t samples_per_packet;

	switch (samplerate)
	{
	    case 32000:
	    case 44100:
	    case 48000:
	    	samples_per_packet = 6;
	    	break;
	    case 88200:
	    case 96000:
	    	samples_per_packet = 12;
	    	break;
	    case 176400:
	    case 192000:
	    	samples_per_packet = 24;
	    	break;
	    default:
	    	return;
	}

	memcpy(&p->dest, dest, ETH_ALEN);
	memcpy(&p->src, src, ETH_ALEN);

	p->vlan_tag = htons(VLAN_TAG);
	p->avtp_vlan_tag = htonl(AVTP_VLAN_TAG);

	p->avtp_subtype = AVTP_IEC61883;
	p->flags1 = STREAM_ID_VALID;

	memcpy(&p->stream_id, stream_id, STREAM_ID_LEN);

	p->stream_data_length = BYTES_PER_SAMPLE *
			                channels *
							samples_per_packet +
							CIP_HEADER_LEN;

	p->cip_0 = CIP_0;
	p->cip_1 = CIP_1;
	p->cip_2 = CIP_2;

	p->data_block_size = channels;

	p->cip_4 = CIP_4;
	p->cip_6 = CIP_6;

	switch (samplerate)
	{
	    case 32000:
	    	p->fdf = 0;
	    	p->syt = 0x08;
	    	break;
	    case 44100:
	    	p->fdf = 1;
	    	p->syt = 0x08;
	    	break;
	    case 48000:
	    	p->fdf = 2;
	    	p->syt = 0x08;
	    	break;
	    case 88200:
	    	p->fdf = 3;
	    	p->syt = 0x10;
	    	break;
	    case 96000:
	    	p->fdf = 4;
	    	p->syt = 0x10;
	    	break;
	    case 176400:
	    	p->fdf = 5;
	    	p->syt = 0x20;
	    	break;
	    case 192000:
	    	p->fdf = 6;
	    	p->syt = 0x20;
	    	break;
	    default:
	    	return;
	}

}
void init_avpdu_header_no_vlan
       (AVPDU_HEADER_NO_VLAN* p,
        uint8_t *dest,
        uint8_t *src,
        uint8_t *stream_id,
        uint8_t channels,
		uint32_t samplerate)
{
	memset(p, 0, sizeof(AVPDU_HEADER_NO_VLAN));

	uint16_t samples_per_packet;

	switch (samplerate)
	{
	    case 32000:
	    case 44100:
	    case 48000:
	    	samples_per_packet = 6;
	    	break;
	    case 88200:
	    case 96000:
	    	samples_per_packet = 12;
	    	break;
	    case 176400:
	    case 192000:
	    	samples_per_packet = 24;
	    	break;
	    default:
	    	return;
	}

	memcpy(&p->dest, dest, ETH_ALEN);
	memcpy(&p->src, src, ETH_ALEN);

	p->avtp_subtype = AVTP_IEC61883;
	p->flags1 = STREAM_ID_VALID;

	memcpy(&p->stream_id, stream_id, STREAM_ID_LEN);

	p->stream_data_length = BYTES_PER_SAMPLE *
			                channels *
							samples_per_packet +
							CIP_HEADER_LEN;

	p->cip_0 = CIP_0;
	p->cip_1 = CIP_1;
	p->cip_2 = CIP_2;

	p->data_block_size = channels;

	p->cip_4 = CIP_4;
	p->cip_6 = CIP_6;

	switch (samplerate)
	{
	    case 32000:
	    	p->fdf = 0;
	    	p->syt = 0x08;
	    	break;
	    case 44100:
	    	p->fdf = 1;
	    	p->syt = 0x08;
	    	break;
	    case 48000:
	    	p->fdf = 2;
	    	p->syt = 0x08;
	    	break;
	    case 88200:
	    	p->fdf = 3;
	    	p->syt = 0x10;
	    	break;
	    case 96000:
	    	p->fdf = 4;
	    	p->syt = 0x10;
	    	break;
	    case 176400:
	    	p->fdf = 5;
	    	p->syt = 0x20;
	    	break;
	    case 192000:
	    	p->fdf = 6;
	    	p->syt = 0x20;
	    	break;
	    default:
	    	return;
	}

}
