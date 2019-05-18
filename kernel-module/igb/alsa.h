#include "igb.h"

#ifndef IGB_ALSA_H
#define IGB_ALSA_H

/* stream direction */

#define MAX_AVB_STREAMS_PER_SUBSTREAM MAX_AVB_STREAMS

typedef struct {
   uint32_t cmd;
   uint32_t substream;    // ALSA substream number
   uint32_t block;        // AVB stream index within ALSA substream
   uint32_t samplerate;   // samplerate of AVB audio unit
   uint64_t stream_id;
   uint64_t dest_mac;
   uint16_t vlan;
   uint16_t index;
} Avb_Cmd;

int snd_avb_probe(struct igb_adapter *adapter, int samplerate);

void snd_avb_remove(struct igb_adapter *adapter);

void snd_avb_receive(struct igb_adapter *adapter);

#endif

