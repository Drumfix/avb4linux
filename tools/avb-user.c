#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#include <poll.h>

#include "mrp_client.h"
#include "maap_client.h"
#include "avpdu.h"

#include <jdksavdecc.h>
#include <../kernel-module/igb/avb-config.h>

#define CLASS_A_ID 6
#define CLASS_A_VLAN 2
#define CLASS_A_PRIORITY 3

char DMAC[] = { 0x91, 0xe0, 0xf0, 0x01, 0x00, 0x00 };  // for ADP and AECP

#define MCAST_BASE MAAP_LOCALLY_ADMINISTRATORED_BASE

uint64_t ENTITY_ID;
uint64_t CONTROLLER_ID;
uint64_t MODEL_ID;
uint64_t GRANDMASTER_ID;

uint64_t REMOTE_TALKER_ID;
uint64_t REMOTE_LISTENER_ID;

uint64_t own_mac_address; 
char MAC[6];

struct jdksavdecc_eui64 entity_guid;
struct jdksavdecc_eui64 model_id;
struct jdksavdecc_eui64 controller_guid;
struct jdksavdecc_eui64 grandmaster_id;

struct jdksavdecc_eui64 f_stream;

struct jdksavdecc_eui64 remote_talker_guid;
struct jdksavdecc_eui64 remote_listener_guid;

struct jdksavdecc_eui48 f_mac;

// NIC index
int ifindex = 0;

#define L2_PACKET_IPG (125000) /* (1) packet every 125 usec */
#define PKT_SZ (100)

// offset of control header in ethernet frame

#define HEADER_OFFSET 14
#define MACLEN 6

#define NUM_STREAMS 16

#define STREAM_PORT_INPUT_BASE_CLUSTER 0
#define STREAM_PORT_INPUT_BASE_MAP 0

#define STREAM_PORT_OUTPUT_BASE_CLUSTER (STREAM_PORT_INPUT_BASE_CLUSTER+(NUM_STREAMS*8))
#define STREAM_PORT_OUTPUT_BASE_MAP (STREAM_PORT_INPUT_BASE_MAP+NUM_STREAMS)

/* some conversion helpers */

uint64_t array6_to_uint64(char *val)
{
    uint64_t ret = 0;

    for (unsigned i=0; i<6; i++)
    {
        uint64_t tmp = (uint64_t)val[i];

        ret = ret + (tmp << (8*(5-i)));
    }

    return ret;
}

void uint64_to_array6(uint64_t src, uint8_t *dest)
{
    memset(dest, 0, 6);

    dest[0] = (src >> (8*0)) & 0xff;
    dest[1] = (src >> (8*1)) & 0xff;
    dest[2] = (src >> (8*2)) & 0xff;
    dest[3] = (src >> (8*3)) & 0xff;
    dest[4] = (src >> (8*4)) & 0xff;
    dest[5] = (src >> (8*5)) & 0xff;
}

void uint64_to_array8(uint64_t src, uint8_t *dest)
{
    memset(dest, 0, 8);

    dest[0] = (src >> (8*0)) & 0xff;
    dest[1] = (src >> (8*1)) & 0xff;
    dest[2] = (src >> (8*2)) & 0xff;
    dest[3] = (src >> (8*3)) & 0xff;
    dest[4] = (src >> (8*4)) & 0xff;
    dest[5] = (src >> (8*5)) & 0xff;
    dest[6] = (src >> (8*6)) & 0xff;
    dest[7] = (src >> (8*7)) & 0xff;
}

uint64_t mac_to_entity_id(uint64_t mac)
{
    return ((mac & 0xffffff000000) << 16) | (0xfffe000000) | (mac & 0xffffff);
}

uint64_t mac_to_controller_id(uint64_t mac)
{
    return ((mac & 0xffffff000000) << 16) | (0xfefe000000) | (mac & 0xffffff);
}

uint64_t mac_to_grandmaster_id(uint64_t mac)
{
    return ((mac & 0xffffff000000) << 16) | (0xfffe000000) | (mac & 0xffffff);
}

uint64_t mac_to_stream_id(uint64_t mac, uint64_t guid)
{
    return (mac << 16) | guid;
}

// our descriptors

// entity descriptor

struct jdksavdecc_descriptor_entity entity;

void my_talker_cb(const struct mrp_talker_ctx ctx) 
{
int i;

printf("talker:\n");
printf("mode %d\n", ctx.mode);
printf("stream: ");
for (i=0;i<8;i++) printf("%2x", ctx.stream_id[i]);
printf("\n");
printf("dst_mac: ");
for (i=0;i<6;i++) printf("%2x", ctx.dst_mac[i]);
printf("\n");
printf("vid: %d\n", ctx.vid);
}

void my_listener_cb(const struct mrp_listener_ctx ctx)
{
int i;

printf("listener:\n");
printf("mode %d\n", ctx.mode);
printf("stream: ");
for (i=0;i<8;i++) printf("%2x", ctx.stream_id[i]);
printf("\n");
printf("listener: ");
for (i=0;i<6;i++) printf("%2x", ctx.listener[i]);
printf("\n");
}

void my_domain_cb(const struct mrp_domain_ctx ctx)
{
printf("domain:\n");
printf("mode %d\n", ctx.mode);
printf("id %d\n", ctx.id);
printf("prio %d\n", ctx.priority);
printf("vid %d\n", ctx.vid);
}


void fill_jdksavdecc_descriptor_entity()
{
   memset(&entity, 0, sizeof(entity));

   entity.descriptor_type = JDKSAVDECC_DESCRIPTOR_ENTITY;
   entity.descriptor_index = 0;
   jdksavdecc_eui64_init_from_uint64(&entity.entity_id, ENTITY_ID);
   jdksavdecc_eui64_init_from_uint64(&entity.entity_model_id, MODEL_ID);

   entity.entity_capabilities = 
      JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_SUPPORTED |
      JDKSAVDECC_ADP_ENTITY_CAPABILITY_CLASS_A_SUPPORTED |
      JDKSAVDECC_ADP_ENTITY_CAPABILITY_GPTP_SUPPORTED |
      JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_IDENTIFY_CONTROL_INDEX_VALID |
      JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_INTERFACE_INDEX_VALID;
   entity.talker_stream_sources = 16;
   entity.talker_capabilities = 
      JDKSAVDECC_ADP_TALKER_CAPABILITY_IMPLEMENTED |
      JDKSAVDECC_ADP_TALKER_CAPABILITY_AUDIO_SOURCE;
   entity.listener_stream_sinks = 16;
   entity.listener_capabilities =
      JDKSAVDECC_ADP_LISTENER_CAPABILITY_IMPLEMENTED |
      JDKSAVDECC_ADP_LISTENER_CAPABILITY_AUDIO_SINK;
   entity.controller_capabilities = 0;
   entity.available_index = 0;
   jdksavdecc_eui64_init_from_uint64(&entity.association_id, 0);
   jdksavdecc_string_set_from_cstr(&entity.entity_name, "AVB");
   entity.vendor_name_string = 0;
   entity.model_name_string = 1;
   jdksavdecc_string_set_from_cstr(&entity.firmware_version, "1.0.0");
   jdksavdecc_string_set_from_cstr(&entity.group_name, "");
   jdksavdecc_string_set_from_cstr(&entity.serial_number, "1");
   entity.configurations_count = 1;
   entity.current_configuration = 0;
}


// configuration descriptor

struct jdksavdecc_descriptor_configuration configuration;
uint16_t configuration_descriptors[16];

void fill_jdksavdecc_descriptor_configuration()
{
   memset(&configuration, 0, sizeof(configuration));

   configuration.descriptor_type = JDKSAVDECC_DESCRIPTOR_CONFIGURATION;
   configuration.descriptor_index = 0;
   jdksavdecc_string_set_from_cstr(&configuration.object_name, "");
   configuration.localized_description = 0xffff;
   configuration.descriptor_counts_count = 8;
   configuration.descriptor_counts_offset  = 74;

   configuration_descriptors[0] = htons(JDKSAVDECC_DESCRIPTOR_AUDIO_UNIT);
   configuration_descriptors[1] = htons(1);
   configuration_descriptors[2] = htons(JDKSAVDECC_DESCRIPTOR_STREAM_INPUT);
   configuration_descriptors[3] = htons(16);  
   configuration_descriptors[4] = htons(JDKSAVDECC_DESCRIPTOR_STREAM_OUTPUT);
   configuration_descriptors[5] = htons(16);
   configuration_descriptors[6] = htons(JDKSAVDECC_DESCRIPTOR_AVB_INTERFACE);
   configuration_descriptors[7] = htons(1);
   configuration_descriptors[8] = htons(JDKSAVDECC_DESCRIPTOR_CLOCK_SOURCE);
   configuration_descriptors[9] = htons(1);
   configuration_descriptors[10] = htons(JDKSAVDECC_DESCRIPTOR_LOCALE);
   configuration_descriptors[11] = htons(1);
   configuration_descriptors[12] = htons(JDKSAVDECC_DESCRIPTOR_STRINGS);
   configuration_descriptors[13] = htons(1);
   configuration_descriptors[14] = htons(JDKSAVDECC_DESCRIPTOR_CLOCK_DOMAIN);
   configuration_descriptors[15] = htons(1);
}

// audio unit descriptor

struct jdksavdecc_descriptor_audio_unit audio_unit;

uint32_t samplerates[6];

void fill_jdksavdecc_descriptor_audio_unit()
{
   audio_unit.descriptor_type = JDKSAVDECC_DESCRIPTOR_AUDIO_UNIT;
   audio_unit.descriptor_index = 0;
   jdksavdecc_string_set_from_cstr(&audio_unit.object_name, "");
   audio_unit.localized_description = 1;
   audio_unit.clock_domain_index = 0;
   audio_unit.number_of_stream_input_ports = 16;
   audio_unit.base_stream_input_port = 0;
   audio_unit.number_of_stream_output_ports = 16;
   audio_unit.base_stream_output_port = 0;
   audio_unit.number_of_external_input_ports = 0;
   audio_unit.base_external_input_port = 0;
   audio_unit.number_of_external_output_ports = 0;
   audio_unit.base_external_output_port = 0;
   audio_unit.number_of_internal_input_ports = 0;
   audio_unit.base_internal_input_port = 0;
   audio_unit.number_of_internal_output_ports = 0;
   audio_unit.base_internal_output_port = 0;
   audio_unit.number_of_controls = 0;
   audio_unit.base_control = 0;
   audio_unit.number_of_signal_selectors = 0;
   audio_unit.base_signal_selector = 0;
   audio_unit.number_of_mixers = 0;
   audio_unit.base_mixer = 0;
   audio_unit.number_of_matrices = 0;
   audio_unit.base_matrix = 0;
   audio_unit.number_of_splitters = 0;
   audio_unit.base_splitter = 0;
   audio_unit.number_of_combiners = 0;
   audio_unit.base_combiner = 0;
   audio_unit.number_of_demultiplexers = 0;
   audio_unit.base_demultiplexer = 0;
   audio_unit.number_of_multiplexers = 0;
   audio_unit.base_multiplexer = 0;
   audio_unit.number_of_transcoders = 0;
   audio_unit.base_transcoder = 0;
   audio_unit.number_of_control_blocks = 0;
   audio_unit.base_control_block = 0;
   audio_unit.current_sampling_rate = 48000;
   audio_unit.sampling_rates_offset = 144;
   audio_unit.sampling_rates_count = 6;

   samplerates[0] = htonl(44100);
   samplerates[1] = htonl(48000);
   samplerates[2] = htonl(88200);
   samplerates[3] = htonl(96000);
   samplerates[4] = htonl(176400);
   samplerates[5] = htonl(192000);
};

struct jdksavdecc_descriptor_stream stream_input[16];
struct jdksavdecc_descriptor_stream stream_output[16];

uint64_t STREAM_INPUT_FORMAT_0 = 0x000800600801A000;
uint64_t STREAM_INPUT_FORMAT_1 = 0x000800600802A000;
uint64_t STREAM_INPUT_FORMAT_2 = 0x000800600803A000;
uint64_t STREAM_INPUT_FORMAT_3 = 0x000800600804A000;
uint64_t STREAM_INPUT_FORMAT_4 = 0x000800600805A000;
uint64_t STREAM_INPUT_FORMAT_5 = 0x000800600806A000;

uint64_t STREAM_INPUT_FORMAT_1_REVERSE =  0x00A0020840000800;

uint64_t STREAM_OUTPUT_FORMAT_0 = 0x000800600801A000;
uint64_t STREAM_OUTPUT_FORMAT_1 = 0x000800600802A000;
uint64_t STREAM_OUTPUT_FORMAT_2 = 0x000800600803A000;
uint64_t STREAM_OUTPUT_FORMAT_3 = 0x000800600804A000;
uint64_t STREAM_OUTPUT_FORMAT_4 = 0x000800600805A000;
uint64_t STREAM_OUTPUT_FORMAT_5 = 0x000800600806A000;

uint64_t STREAM_OUTPUT_FORMAT_1_REVERSE =  0x00A0020840000800;

void fill_jdksavdecc_descriptor_streams()
{
   int i;

   for (i=0; i<16; i++)
   {
      stream_input[i].descriptor_type = JDKSAVDECC_DESCRIPTOR_STREAM_INPUT;
      stream_input[i].descriptor_index = i;
      jdksavdecc_string_set_from_cstr(&stream_input[i].object_name, "");
      stream_input[i].localized_description = -1;
      stream_input[i].clock_domain_index = 0;
      stream_input[i].stream_flags = 
        JDKSAVDECC_DESCRIPTOR_STREAM_FLAG_CLOCK_SYNC_SOURCE |
        JDKSAVDECC_DESCRIPTOR_STREAM_FLAG_CLASS_A;
      jdksavdecc_eui64_init_from_uint64(&stream_input[i].current_format, STREAM_INPUT_FORMAT_1_REVERSE);
      stream_input[i].formats_offset= 132;
      stream_input[i].number_of_formats = 6;
      jdksavdecc_eui64_init_from_uint64(&stream_input[i].backup_talker_entity_id_0, 0);
      stream_input[i].backup_talker_unique_id_0  = 0;
      jdksavdecc_eui64_init_from_uint64(&stream_input[i].backup_talker_entity_id_1, 0);
      stream_input[i].backup_talker_unique_id_1 = 0;
      jdksavdecc_eui64_init_from_uint64(&stream_input[i].backup_talker_entity_id_2, 0);
      stream_input[i].backup_talker_unique_id_2 = 0;
      jdksavdecc_eui64_init_from_uint64(&stream_input[i].backedup_talker_entity_id, 0);
      stream_input[i].backedup_talker_unique = 0;
      stream_input[i].avb_interface_index = 0;
      stream_input[i].buffer_length = 8;

      stream_output[i].descriptor_type = JDKSAVDECC_DESCRIPTOR_STREAM_OUTPUT;
      stream_output[i].descriptor_index = i;
      jdksavdecc_string_set_from_cstr(&stream_output[i].object_name, "");
      stream_output[i].localized_description = -1;
      stream_output[i].clock_domain_index = 0;
      stream_output[i].stream_flags = JDKSAVDECC_DESCRIPTOR_STREAM_FLAG_CLASS_A;
      jdksavdecc_eui64_init_from_uint64(&stream_output[i].current_format, STREAM_OUTPUT_FORMAT_1_REVERSE);
      stream_output[i].formats_offset= 132;
      stream_output[i].number_of_formats = 6;
      jdksavdecc_eui64_init_from_uint64(&stream_output[i].backup_talker_entity_id_0, 0);
      stream_output[i].backup_talker_unique_id_0  = 0;
      jdksavdecc_eui64_init_from_uint64(&stream_output[i].backup_talker_entity_id_1, 0);
      stream_output[i].backup_talker_unique_id_1 = 0;
      jdksavdecc_eui64_init_from_uint64(&stream_output[i].backup_talker_entity_id_2, 0);
      stream_output[i].backup_talker_unique_id_2 = 0;
      jdksavdecc_eui64_init_from_uint64(&stream_output[i].backedup_talker_entity_id, 0);
      stream_output[i].backedup_talker_unique = 0;
      stream_output[i].avb_interface_index = 0;
      stream_output[i].buffer_length = 8;
   }

};

struct jdksavdecc_descriptor_avb_interface avb_interface;

void fill_jdksavdecc_descriptor_avb_interface()
{   
    memset(&avb_interface, 0, sizeof(avb_interface));

    avb_interface.descriptor_type = JDKSAVDECC_DESCRIPTOR_AVB_INTERFACE;
    avb_interface.descriptor_index = 0;
    jdksavdecc_string_set_from_cstr(&avb_interface.object_name, "");
    avb_interface.localized_description = 0;
    memcpy(&avb_interface.mac_address.value, MAC, 6);
    avb_interface.interface_flags = htons
                  (JDKSAVDECC_AVB_INTERFACE_FLAG_GPTP_GRANDMASTER_SUPPORTED |
                   JDKSAVDECC_AVB_INTERFACE_FLAG_GPTP_SUPPORTED |
                   JDKSAVDECC_AVB_INTERFACE_FLAG_SRP_SUPPORTED);
    jdksavdecc_eui64_init_from_uint64(&avb_interface.clock_identity, GRANDMASTER_ID);
    avb_interface.priority1 = 240;
    avb_interface.clock_class = 248;
    avb_interface.offset_scaled_log_variance = 17258;
    avb_interface.clock_accuracy = 248;
    avb_interface.priority2 = 238;
    avb_interface.domain_number = 0;
    avb_interface.log_sync_interval = -2;
    avb_interface.log_announce_interval = 0;
    avb_interface.log_pdelay_interval = 0;
    avb_interface.port_number = 1;
};

struct jdksavdecc_descriptor_clock_source clock_source;

void fill_jdksavdecc_descriptor_clock_source()
{
    clock_source.descriptor_type = JDKSAVDECC_DESCRIPTOR_CLOCK_SOURCE;
    clock_source.descriptor_index = 0;
    jdksavdecc_string_set_from_cstr(&clock_source.object_name, "");
    clock_source.localized_description = 2;
    clock_source.clock_source_flags = 0;
    clock_source.clock_source_type = JDKSAVDECC_CLOCK_SOURCE_TYPE_INTERNAL;
    jdksavdecc_eui64_init_from_uint64(&clock_source.clock_source_identifier, GRANDMASTER_ID);
    clock_source.clock_source_location_type = JDKSAVDECC_DESCRIPTOR_AUDIO_UNIT;
    clock_source.clock_source_location_index = 0;
};

struct jdksavdecc_descriptor_locale locale;

void fill_jdksavdecc_descriptor_locale()
{
    locale.descriptor_type = JDKSAVDECC_DESCRIPTOR_LOCALE;
    locale.descriptor_index = 0;
    jdksavdecc_string_set_from_cstr(&locale.locale_identifier, "en-US");
    locale.number_of_strings = 7;
    locale.base_strings = 0;
};

int running = 1;
unsigned global_available_index = 0;

char *IFNAME = "eth9";

int createSocket(char *name, int *ifindex, struct sockaddr *mac)
{
   int sock;
   sock = socket(AF_PACKET, SOCK_RAW, htons(0x22f0));

   if (sock < 0)
   {
      printf("error in socket\n");
      return -1;
   }

   // Allow address reuse
   int temp = 1;
   if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &temp, sizeof(int)) < 0) 
   {
      printf("Creating sock failed to set reuseaddr\n");
      close(sock);

      return -1;
   }

   struct ifreq ifreq_i;

   memset(&ifreq_i, 0, sizeof(ifreq_i));
   strncpy(ifreq_i.ifr_name, name, IFNAMSIZ-1); //giving name of Interface
 
   if (ioctl(sock, SIOCGIFINDEX, &ifreq_i) < 0)
   {
      printf("error in index ioctl reading"); //getting Index Name
   }
   printf("index=%d\n", ifreq_i.ifr_ifindex);

   *ifindex = ifreq_i.ifr_ifindex;

   // Bind to interface

   struct sockaddr_ll my_addr;
   memset(&my_addr, 0, sizeof(my_addr));
   my_addr.sll_family = AF_PACKET;
   my_addr.sll_protocol = htons(0x22f0);
   my_addr.sll_ifindex = *ifindex;

   if (bind(sock, (struct sockaddr*)&my_addr, sizeof(my_addr)) == -1) 
   {
      printf("Error in bind socket: %s\n", strerror(errno));
      close(sock);

      return -1;
   }

   struct packet_mreq mreq;
   memset(&mreq, 0, sizeof(struct packet_mreq));
   mreq.mr_ifindex = *ifindex;
   mreq.mr_type = PACKET_MR_ALLMULTI;
   mreq.mr_alen = 0;
   memset(&mreq.mr_address, 0, sizeof(mreq.mr_address));

   if (setsockopt(sock, SOL_SOCKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) //getting MAC Address
      printf("error setting setsockopt PACKET_ADD_MEMBERSHIP\n");
   
   struct ifreq ifreq_c;

   memset(&ifreq_c, 0, sizeof(ifreq_c));
   strncpy(ifreq_c.ifr_name, name, IFNAMSIZ-1); //giving name of Interface
 
   if (ioctl(sock, SIOCGIFHWADDR, &ifreq_c) < 0) //getting MAC Address
      printf("error in SIOCGIFHWADDR ioctl reading\n");

   memcpy(mac, &ifreq_c.ifr_hwaddr, sizeof (ifreq_c.ifr_hwaddr));

   return sock;
}


int recvMsg(int sock, char* buffer, int buflen)
{
   struct sockaddr saddr;

   int saddr_len = sizeof (saddr);
 
   //Receive a network packet and copy into buffer
   int ret = recvfrom(sock, buffer, buflen, 0, &saddr, (socklen_t *)&saddr_len);

   if (ret < 0)
   {
      printf("error in reading recvfrom function\n");
   }
   return ret;
}

int sendMsg(int sock, char* buffer, int buflen)
{
   int sendlen;

   struct sockaddr_ll sadr_ll;
   sadr_ll.sll_family = AF_PACKET;
   sadr_ll.sll_ifindex = ifindex;
   sadr_ll.sll_halen = ETH_ALEN;
   sadr_ll.sll_addr[0] = buffer[0];
   sadr_ll.sll_addr[1] = buffer[1];
   sadr_ll.sll_addr[2] = buffer[2];
   sadr_ll.sll_addr[3] = buffer[3];
   sadr_ll.sll_addr[4] = buffer[4];
   sadr_ll.sll_addr[5] = buffer[5];

   sendlen = sendto(sock, buffer , buflen, 0, (const struct sockaddr*)&sadr_ll, sizeof(struct sockaddr_ll));
   if (sendlen < 0)
   {
      printf("error in sending....sendlen=%d....errno=%d\n", sendlen, errno);
   }
   return sendlen;
}

void handle_entity_descriptor(int sock, char *buffer, int len)
{
    entity.available_index = global_available_index;
    jdksavdecc_common_control_header_set_control_data_length(328, buffer, HEADER_OFFSET);
    jdksavdecc_descriptor_entity_write(&entity, buffer, 0x2A, JDKSAVDECC_DESCRIPTOR_ENTITY_LEN + 0x2A);

    sendMsg(sock, buffer, 354);
}

void handle_configuration_descriptor(int sock, char *buffer, int len)
{
    jdksavdecc_common_control_header_set_control_data_length(122, buffer, HEADER_OFFSET);
    jdksavdecc_descriptor_configuration_write(&configuration, buffer, 0x2A, JDKSAVDECC_DESCRIPTOR_CONFIGURATION_LEN + 0x2A);
    memcpy(&buffer[0x74], configuration_descriptors, sizeof(configuration_descriptors));

    sendMsg(sock, buffer, 148);
}

void handle_audio_unit_descriptor(int sock, char *buffer, int len)
{
    jdksavdecc_common_control_header_set_control_data_length(184, buffer, HEADER_OFFSET);
    jdksavdecc_descriptor_audio_unit_write(&audio_unit, buffer, 0x2A, JDKSAVDECC_DESCRIPTOR_AUDIO_UNIT_LEN + 0x2A);
    memcpy(&buffer[0xba], samplerates, sizeof(samplerates));

    sendMsg(sock, buffer, 210);
}

void handle_stream_input_descriptor(int sock, char *buffer, int len)
{
    int i = jdksavdecc_descriptor_stream_get_descriptor_index(buffer, 0x2A);

    jdksavdecc_common_control_header_set_control_data_length(196, buffer, HEADER_OFFSET);
    jdksavdecc_descriptor_stream_write(&stream_input[i], buffer, 0x2A, JDKSAVDECC_DESCRIPTOR_STREAM_LEN + 0x2A);
    memcpy(&buffer[0xae], &STREAM_INPUT_FORMAT_0, sizeof(STREAM_INPUT_FORMAT_0));
    memcpy(&buffer[0xae + 8], &STREAM_INPUT_FORMAT_1, sizeof(STREAM_INPUT_FORMAT_1));
    memcpy(&buffer[0xae + 16], &STREAM_INPUT_FORMAT_2, sizeof(STREAM_INPUT_FORMAT_2));
    memcpy(&buffer[0xae + 24], &STREAM_INPUT_FORMAT_3, sizeof(STREAM_INPUT_FORMAT_3));
    memcpy(&buffer[0xae + 32], &STREAM_INPUT_FORMAT_4, sizeof(STREAM_INPUT_FORMAT_4));
    memcpy(&buffer[0xae + 40], &STREAM_INPUT_FORMAT_5, sizeof(STREAM_INPUT_FORMAT_5));

    sendMsg(sock, buffer, 222);
}

void handle_stream_output_descriptor(int sock, char *buffer, int len)
{
    int i = jdksavdecc_descriptor_stream_get_descriptor_index(buffer, 0x2A);

    jdksavdecc_common_control_header_set_control_data_length(196, buffer, HEADER_OFFSET);
    jdksavdecc_descriptor_stream_write(&stream_output[i], buffer, 0x2A, JDKSAVDECC_DESCRIPTOR_STREAM_LEN + 0x2A);
    memcpy(&buffer[0xae], &STREAM_OUTPUT_FORMAT_0, sizeof(STREAM_OUTPUT_FORMAT_0));
    memcpy(&buffer[0xae + 8], &STREAM_OUTPUT_FORMAT_1, sizeof(STREAM_OUTPUT_FORMAT_1));
    memcpy(&buffer[0xae + 16], &STREAM_OUTPUT_FORMAT_2, sizeof(STREAM_OUTPUT_FORMAT_2));
    memcpy(&buffer[0xae + 24], &STREAM_OUTPUT_FORMAT_3, sizeof(STREAM_OUTPUT_FORMAT_3));
    memcpy(&buffer[0xae + 32], &STREAM_OUTPUT_FORMAT_4, sizeof(STREAM_OUTPUT_FORMAT_4));
    memcpy(&buffer[0xae + 40], &STREAM_OUTPUT_FORMAT_5, sizeof(STREAM_OUTPUT_FORMAT_5));

    sendMsg(sock, buffer, 222);
}

void handle_avb_interface_descriptor(int sock, char *buffer, int len)
{
    jdksavdecc_common_control_header_set_control_data_length(114, buffer, HEADER_OFFSET);
    jdksavdecc_descriptor_avb_interface_write(&avb_interface, buffer, 0x2A, JDKSAVDECC_DESCRIPTOR_AVB_INTERFACE_LEN + 0x2A);

    sendMsg(sock, buffer, 140);
}

void handle_clock_source_descriptor(int sock, char *buffer, int len)
{
    jdksavdecc_common_control_header_set_control_data_length(102, buffer, HEADER_OFFSET);
    jdksavdecc_descriptor_clock_source_write(&clock_source, buffer, 0x2A, JDKSAVDECC_DESCRIPTOR_CLOCK_SOURCE_LEN + 0x2A);

    sendMsg(sock, buffer, 128);
}

void handle_locale_descriptor(int sock, char *buffer, int len)
{
    jdksavdecc_common_control_header_set_control_data_length(88, buffer, HEADER_OFFSET);
    jdksavdecc_descriptor_locale_write(&locale, buffer, 0x2A, JDKSAVDECC_DESCRIPTOR_LOCALE_LEN + 0x2A);

    sendMsg(sock, buffer, 114);
}

void handle_clock_domain_descriptor(int sock, char *buffer, int len)
{
    int i;
    struct jdksavdecc_descriptor_clock_domain cd;

    cd.descriptor_type = JDKSAVDECC_DESCRIPTOR_CLOCK_DOMAIN;
    cd.descriptor_index = 0;
    jdksavdecc_string_set_from_cstr(&cd.object_name, "");
    cd.localized_description = -1;
    cd.clock_source_index = 0;
    cd.clock_sources_offset = 76;
    cd.clock_sources_count = 1;

    jdksavdecc_common_control_header_set_control_data_length(94, buffer, HEADER_OFFSET);
    jdksavdecc_descriptor_clock_domain_write(&cd, buffer, 0x2A, JDKSAVDECC_DESCRIPTOR_CLOCK_DOMAIN_LEN + 0x2A);

    /* clock source 0 */

    buffer[0x76] = 0;
    buffer[0x77] = 0;

    sendMsg(sock, buffer, 120);
}

void handle_strings_descriptor(int sock, char *buffer, int len)
{
    struct jdksavdecc_descriptor_strings strings;

    memset(&strings, 0, sizeof(strings));

    strings.descriptor_type = JDKSAVDECC_DESCRIPTOR_STRINGS;
    strings.descriptor_index = 0;
    jdksavdecc_string_set_from_cstr(&strings.string_0, "Drumfix");
    jdksavdecc_string_set_from_cstr(&strings.string_1, "Linux AVB");
    jdksavdecc_string_set_from_cstr(&strings.string_2, "Internal");
    jdksavdecc_string_set_from_cstr(&strings.string_3, "");
    jdksavdecc_string_set_from_cstr(&strings.string_4, "");
    jdksavdecc_string_set_from_cstr(&strings.string_5, "");
    jdksavdecc_string_set_from_cstr(&strings.string_6, "");

    jdksavdecc_common_control_header_set_control_data_length(468, buffer, HEADER_OFFSET);
    jdksavdecc_descriptor_strings_write(&strings, buffer, 0x2A, JDKSAVDECC_DESCRIPTOR_STRINGS_LEN + 0x2A);

    sendMsg(sock, buffer, 494);
}

void handle_stream_port_input_descriptor(int sock, char *buffer, int len)
{
    struct jdksavdecc_descriptor_stream_port spi;

    memset(&spi, 0, sizeof(spi));

    spi.descriptor_type = JDKSAVDECC_DESCRIPTOR_STREAM_PORT_INPUT;
    spi.descriptor_index = jdksavdecc_descriptor_stream_get_descriptor_index(buffer, 0x2A);
    spi.clock_domain_index = 0;
    spi.port_flags = 0;
    spi.number_of_controls = 0;
    spi.base_control = 0;
    spi.number_of_clusters = 8;
    spi.base_cluster = STREAM_PORT_INPUT_BASE_CLUSTER + 8 * spi.descriptor_index;
    spi.number_of_maps = 1;
    spi.base_map = STREAM_PORT_INPUT_BASE_MAP + spi.descriptor_index;
    
    jdksavdecc_common_control_header_set_control_data_length(36, buffer, HEADER_OFFSET);
    jdksavdecc_descriptor_stream_port_write(&spi, buffer, 0x2A, JDKSAVDECC_DESCRIPTOR_STREAM_PORT_LEN + 0x2A);

    sendMsg(sock, buffer, 64);
}

void handle_stream_port_output_descriptor(int sock, char *buffer, int len)
{
    struct jdksavdecc_descriptor_stream_port spo;

    memset(&spo, 0, sizeof(spo));

    spo.descriptor_type = JDKSAVDECC_DESCRIPTOR_STREAM_PORT_INPUT;
    spo.descriptor_index = jdksavdecc_descriptor_stream_get_descriptor_index(buffer, 0x2A);
    spo.clock_domain_index = 0;
    spo.port_flags = JDKSAVDECC_DESCRIPTOR_STREAM_FLAG_CLOCK_SYNC_SOURCE;
    spo.number_of_controls = 0;
    spo.base_control = 0;
    spo.number_of_clusters = 8;
    spo.base_cluster = STREAM_PORT_OUTPUT_BASE_CLUSTER + 8 * spo.descriptor_index;
    spo.number_of_maps = 1;
    spo.base_map = STREAM_PORT_OUTPUT_BASE_MAP + spo.descriptor_index;
    
    jdksavdecc_common_control_header_set_control_data_length(36, buffer, HEADER_OFFSET);
    jdksavdecc_descriptor_stream_port_write(&spo, buffer, 0x2A, JDKSAVDECC_DESCRIPTOR_STREAM_PORT_LEN + 0x2A);

    sendMsg(sock, buffer, 64);
}

void handle_audio_cluster_descriptor(int sock, char *buffer, int len)
{
    struct jdksavdecc_descriptor_audio_cluster ac;

    memset(&ac, 0, sizeof(ac));

    ac.descriptor_type = JDKSAVDECC_DESCRIPTOR_AUDIO_CLUSTER;
    ac.descriptor_index = jdksavdecc_descriptor_stream_get_descriptor_index(buffer, 0x2A);
    struct jdksavdecc_string object_name;
    jdksavdecc_string_set_from_cstr(&ac.object_name, "");
    ac.localized_description = -1;
    ac.signal_type = -1;
    ac.signal_index = 0;
    ac.signal_output = 0;
    ac.path_latency = 0;
    ac.block_latency = 0;
    ac.channel_count = 1;
    ac.format = JDKSAVDECC_AUDIO_CLUSTER_FORMAT_MBLA;

    jdksavdecc_common_control_header_set_control_data_length(103, buffer, HEADER_OFFSET);
    jdksavdecc_descriptor_audio_cluster_write(&ac, buffer, 0x2A, JDKSAVDECC_DESCRIPTOR_AUDIO_CLUSTER_LEN + 0x2A);

    sendMsg(sock, buffer, 129);
}

void handle_audio_map_descriptor(int sock, char *buffer, int len)
{
    struct jdksavdecc_descriptor_audio_map am;
    struct jdksavdecc_audio_mapping* amap = (struct jdksavdecc_audio_mapping*)buffer+50;
    int i;

    memset(&am, 0, sizeof(am));

    am.descriptor_type = JDKSAVDECC_DESCRIPTOR_AUDIO_MAP;
    am.descriptor_index = jdksavdecc_descriptor_stream_get_descriptor_index(buffer, 0x2A);
    am.mappings_offset = 8;
    am.number_of_mappings = 8;

    jdksavdecc_common_control_header_set_control_data_length(88, buffer, HEADER_OFFSET);
    jdksavdecc_descriptor_audio_map_write(&am, buffer, 0x2A, JDKSAVDECC_DESCRIPTOR_AUDIO_MAP_LEN + 0x2A);

    for (i=0; i<8; i++)
    {
        amap[i].mapping_stream_index = htons(am.descriptor_index);
        amap[i].mapping_stream_channel = htons(i);
        amap[i].mapping_cluster_offset = htons(i);
        amap[i].mapping_cluster_channel = 0;
    }

    sendMsg(sock, buffer, 114);
}

void handle_no_such_descriptor(int sock, char *buffer, int len)
{
    jdksavdecc_common_control_header_set_status(JDKSAVDECC_AEM_STATUS_NO_SUCH_DESCRIPTOR, buffer, HEADER_OFFSET);
    sendMsg(sock, buffer, len);
}

void handle_aem_command_read_descriptor(int sock, char *buffer, int len)
{
    switch (jdksavdecc_descriptor_entity_get_descriptor_type(buffer, 0x2A))
    {
       case JDKSAVDECC_DESCRIPTOR_ENTITY:
       {
          handle_entity_descriptor(sock, buffer, len);
          break;
       }
       case JDKSAVDECC_DESCRIPTOR_CONFIGURATION:
       {
          handle_configuration_descriptor(sock, buffer, len);
          break;
       }
       case JDKSAVDECC_DESCRIPTOR_AUDIO_UNIT:
       {
          handle_audio_unit_descriptor(sock, buffer, len);
          break;
       }
       case JDKSAVDECC_DESCRIPTOR_STREAM_INPUT:
       {
          handle_stream_input_descriptor(sock, buffer, len);
          break;
       }
       case JDKSAVDECC_DESCRIPTOR_STREAM_OUTPUT:
       {
          handle_stream_output_descriptor(sock, buffer, len);
          break;
       }
       case JDKSAVDECC_DESCRIPTOR_AVB_INTERFACE:
       {
          handle_avb_interface_descriptor(sock, buffer, len);
          break;
       }
       case JDKSAVDECC_DESCRIPTOR_CLOCK_SOURCE:
       {
          handle_clock_source_descriptor(sock, buffer, len);
          break;
       }
       case JDKSAVDECC_DESCRIPTOR_LOCALE:
       {
          handle_locale_descriptor(sock, buffer, len);
          break;
       }
       case JDKSAVDECC_DESCRIPTOR_STRINGS:
       {
          handle_strings_descriptor(sock, buffer, len);
          break;
       }
       case JDKSAVDECC_DESCRIPTOR_STREAM_PORT_INPUT:
       {
          handle_stream_port_input_descriptor(sock, buffer, len);
          break;
       }
       case JDKSAVDECC_DESCRIPTOR_STREAM_PORT_OUTPUT:
       {
          handle_stream_port_output_descriptor(sock, buffer, len);
          break;
       }
       case JDKSAVDECC_DESCRIPTOR_CLOCK_DOMAIN:
       {
          handle_clock_domain_descriptor(sock, buffer, len);
          break;
       }
       case JDKSAVDECC_DESCRIPTOR_AUDIO_CLUSTER:
       {
          handle_audio_cluster_descriptor(sock, buffer, len);
          break;
       }
       case JDKSAVDECC_DESCRIPTOR_AUDIO_MAP:
       {
          handle_audio_map_descriptor(sock, buffer, len);
          break;
       }
       default:
          handle_no_such_descriptor(sock, buffer, len);
          break;
    }
}

void handle_unimplemented_aem_command(int sock, char *buffer, int len)
{         
    jdksavdecc_common_control_header_set_status(JDKSAVDECC_AEM_STATUS_NOT_IMPLEMENTED, buffer, HEADER_OFFSET);

    sendMsg(sock, buffer, len);
}

void handle_aem_command(int sock, char *buffer, int len)
{
    memcpy(buffer, &buffer[MACLEN], MACLEN);
    memcpy(&buffer[MACLEN], MAC, MACLEN);
    jdksavdecc_common_control_header_set_control_data(JDKSAVDECC_AECP_MESSAGE_TYPE_AEM_RESPONSE, buffer, HEADER_OFFSET);
    jdksavdecc_common_control_header_set_status(JDKSAVDECC_AEM_STATUS_SUCCESS, buffer, HEADER_OFFSET);

   switch (jdksavdecc_aecpdu_aem_get_command_type(buffer, HEADER_OFFSET))

   {
      case JDKSAVDECC_AEM_COMMAND_REGISTER_UNSOLICITED_NOTIFICATION:
      {
        handle_unimplemented_aem_command(sock, buffer, len);
        break;
      }
      case JDKSAVDECC_AEM_COMMAND_READ_DESCRIPTOR:
      {
        handle_aem_command_read_descriptor(sock, buffer, len);
        break;
      }     
      case JDKSAVDECC_AEM_COMMAND_GET_STREAM_FORMAT:
      {
        handle_unimplemented_aem_command(sock, buffer, len);
        break;
      }
      case JDKSAVDECC_AEM_COMMAND_GET_SAMPLING_RATE:
      {
        handle_unimplemented_aem_command(sock, buffer, len);
        break;
      }
      default:
      { 
        handle_unimplemented_aem_command(sock, buffer, len);
        break;
      }
   }
}

void handle_aecp_vendor_unique_command(int sock, char *buffer, int len)
{
    memcpy(buffer, &buffer[MACLEN], MACLEN);
    memcpy(&buffer[MACLEN], MAC, MACLEN);

    jdksavdecc_common_control_header_set_control_data
      (JDKSAVDECC_AECP_MESSAGE_TYPE_VENDOR_UNIQUE_RESPONSE, buffer, HEADER_OFFSET);

    handle_unimplemented_aem_command(sock, buffer, len);
}

void handle_aecp(int sock, char *buffer, int len)
{

    struct jdksavdecc_eui64 target_id = jdksavdecc_common_control_header_get_stream_id(buffer, HEADER_OFFSET);
    uint64_t target_id_a = jdksavdecc_eui64_convert_to_uint64(&target_id);

    switch (jdksavdecc_common_control_header_get_control_data(buffer, HEADER_OFFSET))
    {
       case JDKSAVDECC_AECP_MESSAGE_TYPE_AEM_COMMAND:
       {
         if (target_id_a != ENTITY_ID)
         {
            return;
         }
         handle_aem_command(sock, buffer, len);
         break;
       }
       case JDKSAVDECC_AECP_MESSAGE_TYPE_VENDOR_UNIQUE_COMMAND:
       {
         if (target_id_a != CONTROLLER_ID)
         {
            return;
         }
         handle_aecp_vendor_unique_command(sock, buffer, len);
         break;
       }
       default:
         break;
    }
}

void handle_unimplemented_acmp_command(int sock, char *buffer, int len)
{
    uint8_t command_response = jdksavdecc_common_control_header_get_control_data(buffer, HEADER_OFFSET);

    command_response++;

    jdksavdecc_common_control_header_set_control_data(command_response, buffer, HEADER_OFFSET);

    jdksavdecc_common_control_header_set_status(JDKSAVDECC_ACMP_STATUS_INCOMPATIBLE_REQUEST, buffer, HEADER_OFFSET);

    sendMsg(sock, buffer, len);
}

void handle_acmp_connect_tx_command(int sock, char *buffer, int len)
{
    struct jdksavdecc_eui64 talker_guid = jdksavdecc_acmpdu_get_talker_entity_id(buffer, HEADER_OFFSET);

    if (jdksavdecc_eui64_compare(&talker_guid, &entity_guid))
    {
       return;
    }

    memcpy(&buffer[MACLEN], MAC, MACLEN);

    jdksavdecc_common_control_header_set_control_data(JDKSAVDECC_ACMP_MESSAGE_TYPE_CONNECT_TX_RESPONSE, buffer, HEADER_OFFSET);
    jdksavdecc_common_control_header_set_status(JDKSAVDECC_ACMP_STATUS_SUCCESS, buffer, HEADER_OFFSET);

    struct jdksavdecc_eui64 stream_id;

    jdksavdecc_eui64_init_from_uint64(&stream_id, mac_to_stream_id(own_mac_address, 0));
    jdksavdecc_common_control_header_set_stream_id(stream_id, buffer, HEADER_OFFSET);
    
    struct jdksavdecc_eui48 dst_mac;

    jdksavdecc_eui48_init_from_uint64(&dst_mac, MCAST_BASE);
    jdksavdecc_acmpdu_set_stream_dest_mac(dst_mac, buffer, HEADER_OFFSET);

    jdksavdecc_acmpdu_set_connection_count(1, buffer, HEADER_OFFSET);
    jdksavdecc_acmpdu_set_stream_vlan_id(2, buffer, HEADER_OFFSET);

    sendMsg(sock, buffer, len);
}

void handle_acmp_connect_tx_response(int sock, char *buffer, int len)
{
    struct jdksavdecc_eui64 talker_guid = jdksavdecc_acmpdu_get_talker_entity_id(buffer, HEADER_OFFSET);

    printf("talker %lu, entity %lu\n",
    		jdksavdecc_eui64_convert_to_uint64(&talker_guid),
			jdksavdecc_eui64_convert_to_uint64(&entity_guid));
/*
    if (jdksavdecc_eui64_compare(&talker_guid, &entity_guid))
    {
       return;
    }
*/
    memcpy(&buffer[MACLEN], MAC, MACLEN);

    jdksavdecc_common_control_header_set_control_data(JDKSAVDECC_ACMP_MESSAGE_TYPE_CONNECT_RX_RESPONSE, buffer, HEADER_OFFSET);
    jdksavdecc_common_control_header_set_status(JDKSAVDECC_ACMP_STATUS_SUCCESS, buffer, HEADER_OFFSET);

    sendMsg(sock, buffer, len);
}

void handle_acmp_get_rx_state_command(int sock, char *buffer, int len)
{
    struct jdksavdecc_eui64 listener_guid = jdksavdecc_acmpdu_get_listener_entity_id(buffer, HEADER_OFFSET);

    if (jdksavdecc_eui64_compare(&listener_guid, &entity_guid))
    {
       return;
    }

    memcpy(&buffer[MACLEN], MAC, MACLEN);

    jdksavdecc_common_control_header_set_control_data(JDKSAVDECC_ACMP_MESSAGE_TYPE_GET_RX_STATE_RESPONSE, buffer, HEADER_OFFSET);
    jdksavdecc_common_control_header_set_status(JDKSAVDECC_ACMP_STATUS_SUCCESS, buffer, HEADER_OFFSET);

    struct jdksavdecc_eui64 stream_id;

    uint64_t uint64_remote_stream_id = mac_to_stream_id(AVB_DEVICE_SOURCE_MAC, 0);

    jdksavdecc_eui64_init_from_uint64(&stream_id, uint64_remote_stream_id);
    jdksavdecc_common_control_header_set_stream_id(stream_id, buffer, HEADER_OFFSET);
    
    struct jdksavdecc_eui64 talker_entity_id;

    uint64_t uint64_remote_talker_entity_id = mac_to_entity_id(AVB_DEVICE_SOURCE_MAC);

    jdksavdecc_eui64_init_from_uint64(&talker_entity_id, uint64_remote_talker_entity_id);
    jdksavdecc_acmpdu_set_talker_entity_id(talker_entity_id, buffer, HEADER_OFFSET);

    struct jdksavdecc_eui48 dst_mac;

    uint64_t uint64_remote_destination_mac = AVB_DEVICE_TALKER_MAC_BASE;

    jdksavdecc_eui48_init_from_uint64(&dst_mac, uint64_remote_destination_mac);
    jdksavdecc_acmpdu_set_stream_dest_mac(dst_mac, buffer, HEADER_OFFSET);

    jdksavdecc_acmpdu_set_connection_count(1, buffer, HEADER_OFFSET);
    jdksavdecc_acmpdu_set_stream_vlan_id(2, buffer, HEADER_OFFSET);

    sendMsg(sock, buffer, len);
}

void handle_acmp(int sock, char *buffer, int len)
{
    switch (jdksavdecc_common_control_header_get_control_data(buffer, HEADER_OFFSET))
    {
       case JDKSAVDECC_ACMP_MESSAGE_TYPE_CONNECT_TX_COMMAND:
       {
         handle_acmp_connect_tx_command(sock, buffer, len);
         break;
       }
       case JDKSAVDECC_ACMP_MESSAGE_TYPE_CONNECT_TX_RESPONSE:
       {
         handle_acmp_connect_tx_response(sock, buffer, len);
         break;
       }
       case JDKSAVDECC_ACMP_MESSAGE_TYPE_DISCONNECT_TX_COMMAND:
       {
         //handle_acmp_disconnect_tx_command(sock, buffer, len);
         break;
       }
       case JDKSAVDECC_ACMP_MESSAGE_TYPE_DISCONNECT_TX_RESPONSE:
       {
         //handle_acmp_disconnect_tx_command(sock, buffer, len);
         break;
       }
       case JDKSAVDECC_ACMP_MESSAGE_TYPE_GET_TX_STATE_COMMAND:
       {
         //handle_acmp_get_tx_state_command(sock, buffer, len);
         break;
       }
       case JDKSAVDECC_ACMP_MESSAGE_TYPE_GET_TX_STATE_RESPONSE:
       {
         //handle_acmp_get_tx_state_response(sock, buffer, len);
         break;
       }
       case JDKSAVDECC_ACMP_MESSAGE_TYPE_CONNECT_RX_COMMAND:
       {
         //handle_acmp_connect_rx_command(sock, buffer, len);
         break;
       }
       case JDKSAVDECC_ACMP_MESSAGE_TYPE_CONNECT_RX_RESPONSE:
       {
         //handle_acmp_connect_rx_response(sock, buffer, len);
         break;
       }
       case JDKSAVDECC_ACMP_MESSAGE_TYPE_DISCONNECT_RX_COMMAND:
       {
         //handle_acmp_disconnect_rx_command(sock, buffer, len);
         break;
       }
       case JDKSAVDECC_ACMP_MESSAGE_TYPE_DISCONNECT_RX_RESPONSE:
       {
         //handle_acmp_disconnect_rx_response(sock, buffer, len);
         break;
       }
       case JDKSAVDECC_ACMP_MESSAGE_TYPE_GET_RX_STATE_COMMAND:
       {
         //handle_acmp_get_rx_state_command(sock, buffer, len);
         break;
       }
       case JDKSAVDECC_ACMP_MESSAGE_TYPE_GET_RX_STATE_RESPONSE:
       {
         //handle_acmp_get_rx_state_response(sock, buffer, len);
         break;
       }
       case JDKSAVDECC_ACMP_MESSAGE_TYPE_GET_TX_CONNECTION_COMMAND:
       {
         //handle_acmp_get_tx_connection_command(sock, buffer, len);
         break;
       }
       case JDKSAVDECC_ACMP_MESSAGE_TYPE_GET_TX_CONNECTION_RESPONSE:
       {
         //handle_acmp_get_tx_connection_response(sock, buffer, len);
         break;
       }
       default:
         break;
    }
}

void handle_avpdu(AVPDU_HEADER *p)
{

}

void handle_entity_discover()
{

}

void handle_entity_available(char *buffer)
{

}

void handle_entity_departing(char *buffer)
{

}

void handle_aem(int sock, char *buffer, int len)
{
    int cmd = jdksavdecc_common_control_header_get_control_data(buffer, HEADER_OFFSET);

    switch (cmd)
    {
       case JDKSAVDECC_ADP_MESSAGE_TYPE_ENTITY_DISCOVER:
         handle_entity_discover();
         break;
       case JDKSAVDECC_ADP_MESSAGE_TYPE_ENTITY_AVAILABLE:
         handle_entity_available(buffer);
         break;
       case JDKSAVDECC_ADP_MESSAGE_TYPE_ENTITY_DEPARTING:
         handle_entity_departing(buffer);
         break;
       default:
         printf("handle_aem_command: invalid command %d\n", cmd);
    }
}

void *handle_22f0_msg(void *arg)
{
     int sock = *(int *)arg;

     #define MAX_MSGLEN 1500

     int res = 0;

     char *msg = malloc(MAX_MSGLEN);

     size_t COMMON_HEADER_START = 14;
 
     while (running)
     {
        res = recvMsg(sock, msg, MAX_MSGLEN);

        if (res >= 0)
        {
           uint8_t  subtype =jdksavdecc_common_control_header_get_subtype(msg, COMMON_HEADER_START);

           if (jdksavdecc_common_control_header_get_subtype(msg, COMMON_HEADER_START))
           {
           //printf("msgtype = %02x\n", jdksavdecc_common_control_header_get_subtype(msg, COMMON_HEADER_START));
           }

           switch (jdksavdecc_common_control_header_get_subtype(msg, COMMON_HEADER_START))
           {
              case 0x7a:
              {
                // discovery
            	//printf("msgtype = %x\n", subtype);
                handle_aem(sock, msg, res);
                break;
              }
              case 0x7b:
              {
              	//printf("msgtype = %x\n", subtype);
                handle_aecp(sock, msg, res);
                break;
              }
              case 0x7c:
              {
              	//printf("msgtype = %x\n", subtype);
                handle_acmp(sock, msg, res);
                break;
              }
              case 0x7e:
              {
                // maap
                //printf("msgtype = %x\n", subtype);
                break;
              }
              case 0:
            	  handle_avpdu((AVPDU_HEADER *)msg);
            	  break;
              default:
                //other;
                break;
            }
        }     
     }
     return NULL;
}

int main(int argc, char **argv)
{
   struct sockaddr mac;
   uint64_t mac_address;

   uint32_t samplerate = 48000;
   uint32_t channels_per_stream = 8;
   uint32_t bytes_per_sample = 4;
   uint32_t samples_per_interval = 6;

   if (argc < 3)
   {
      printf("usage: avb-user <name of ethernet interface> <samplerate>\n");
      exit(1);
   }

   int sock = createSocket(argv[1], &ifindex, &mac);

   samplerate = atoi(argv[2]);

   switch (samplerate)
   {
       case 44100:
       case 48000:
          samples_per_interval = 6;
          break;
       case 88200:
       case 96000:
          samples_per_interval = 12;
          break;
       case 176400:
       case 192000:
          samples_per_interval = 24;
          break;
       default:
          printf("Unsuppoerted samplerate %d\n", samplerate);
          exit(1);
     }

   /* initialize MAC address */

   memcpy(MAC, &mac.sa_data, MACLEN);

   mac_address = array6_to_uint64(MAC);

   ENTITY_ID = mac_to_entity_id(mac_address);
   CONTROLLER_ID = mac_to_controller_id(mac_address);
   MODEL_ID = 1;
   GRANDMASTER_ID = mac_to_entity_id(mac_address);

   REMOTE_TALKER_ID = mac_to_entity_id(mac_address);
   REMOTE_LISTENER_ID = mac_to_entity_id(mac_address);

   uint8_t ix_stream[8];
   uint8_t ix_mac[6];

   uint8_t ox_stream[8];
   uint8_t ox_mac[6];

   uint64_t F_STREAM = 0xFFFFFFFFFFFFFFFF;
   uint64_t F_MAC = 0xFFFFFFFFFFFF;

   jdksavdecc_eui64_init_from_uint64(&entity_guid, ENTITY_ID);
   jdksavdecc_eui64_init_from_uint64(&controller_guid, CONTROLLER_ID);
   jdksavdecc_eui64_init_from_uint64(&grandmaster_id, GRANDMASTER_ID);

   jdksavdecc_eui64_init_from_uint64(&remote_talker_guid, REMOTE_TALKER_ID);
   jdksavdecc_eui64_init_from_uint64(&remote_listener_guid, REMOTE_LISTENER_ID);

   jdksavdecc_eui64_init_from_uint64(&f_stream, F_STREAM);
   jdksavdecc_eui48_init_from_uint64(&f_mac, F_MAC);

   int rc;

   fill_jdksavdecc_descriptor_entity();
   fill_jdksavdecc_descriptor_configuration();
   fill_jdksavdecc_descriptor_audio_unit();
   fill_jdksavdecc_descriptor_streams();
   fill_jdksavdecc_descriptor_avb_interface();
   fill_jdksavdecc_descriptor_clock_source();
   fill_jdksavdecc_descriptor_locale();

   /* initialize our ids as used by jdksavdecc */

   /* initialize entity_id */

   jdksavdecc_eui64_init_from_uint64(&entity_guid, ENTITY_ID);

   /* initialize model_id */

   jdksavdecc_eui64_init_from_uint64(&model_id, MODEL_ID);

   /* initialize grandmaster_id */

   jdksavdecc_eui64_init_from_uint64(&grandmaster_id, GRANDMASTER_ID);

   /* initialize controller id */

   jdksavdecc_eui64_init_from_uint64(&controller_guid, CONTROLLER_ID);

   uint64_to_array6(AVB_DEVICE_TALKER_MAC_BASE, ix_mac);
   uint64_to_array8(AVB_DEVICE_SOURCE_MAC << 16, ix_stream);

   uint64_to_array6(MCAST_BASE, ox_mac);
   uint64_to_array8(own_mac_address << 16, ox_stream);

   /* open igb_avb */

   int fd = open("/dev/igb_avb", O_RDWR);

   if (fd < 0)
   {
       printf("Could not open igb_avb.\n");
       exit(1);
   }

   /* maap 
      ignore for now. we use the locally administered MAAP MAC pool instead

   struct maap_record data;

   data.base = MCAST_BASE;
   data.count = NUM_STREAMS;

   rc = maap_client_init(&data);
   if (rc < 0)
   {
      printf("Could not acquire multicast range for talker streams.\n");
      exit(1);
   }
   */

   // mrp setup

   int halt_mrp = 0;

   mrp_client_init(my_talker_cb,
                   my_listener_cb,
                   my_domain_cb);

   mrp_connect(&halt_mrp);

   struct mrp_domain_attr attr;

   attr.id = CLASS_A_ID;
   attr.priority = CLASS_A_PRIORITY;
   attr.vid = CLASS_A_VLAN;

   rc = mrp_register_domain(&attr);
   printf("rc = %d, mrp_register_domain\n", rc);

   rc = mrp_join_vlan(&attr);      
   printf("rc = %d, mrp_join_vlan\n", rc);

   char buffer[128];

   struct pollfd fdset[1];

   struct jdksavdecc_adpdu_common_control_header adp_header;

   adp_header.cd = 1;
   adp_header.subtype = JDKSAVDECC_SUBTYPE_ADP;
   adp_header.sv = 0;
   adp_header.version = 0;
   adp_header.message_type = JDKSAVDECC_ADP_MESSAGE_TYPE_ENTITY_AVAILABLE;
   adp_header.valid_time = 10;
   adp_header.control_data_length = 56;
   adp_header.entity_id = entity_guid;

   struct jdksavdecc_adpdu adpdu;

   adpdu.header = adp_header;
   adpdu.entity_model_id = model_id;
   adpdu.entity_capabilities = 
      JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_SUPPORTED |
      JDKSAVDECC_ADP_ENTITY_CAPABILITY_CLASS_A_SUPPORTED |
      JDKSAVDECC_ADP_ENTITY_CAPABILITY_GPTP_SUPPORTED |
      JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_IDENTIFY_CONTROL_INDEX_VALID |
      JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_INTERFACE_INDEX_VALID;
   adpdu.talker_stream_sources = 16;
   adpdu.talker_capabilities = 
      JDKSAVDECC_ADP_TALKER_CAPABILITY_IMPLEMENTED |
      JDKSAVDECC_ADP_TALKER_CAPABILITY_AUDIO_SOURCE;
   adpdu.listener_stream_sinks = 16;
   adpdu.listener_capabilities =
      JDKSAVDECC_ADP_LISTENER_CAPABILITY_IMPLEMENTED |
      JDKSAVDECC_ADP_LISTENER_CAPABILITY_AUDIO_SINK;
   adpdu.controller_capabilities = 0;
   adpdu.available_index = 0;
   jdksavdecc_eui64_init_from_uint64(&adpdu.gptp_grandmaster_id, GRANDMASTER_ID);
   adpdu.gptp_domain_number = 0;
   adpdu.reserved0 = 0;
   adpdu.identify_control_index = 0;
   adpdu.interface_index = 0;
   jdksavdecc_eui64_init_from_uint64(&adpdu.association_id, 0);
   adpdu.reserved1 = 0;
   
   if (sock >= 0)
   {

      pthread_t control_thread_id;

      pthread_create(&control_thread_id, NULL, handle_22f0_msg, &sock);

      printf("\nMac: %02x-%02x-%02x-%02x-%02x-%02x\n",
          (uint8_t)mac.sa_data[0],
          (uint8_t)mac.sa_data[1],
          (uint8_t)mac.sa_data[2],
          (uint8_t)mac.sa_data[3],
          (uint8_t)mac.sa_data[4],
          (uint8_t)mac.sa_data[5]);

      fdset[0].fd = sock;
      fdset[0].events = POLLIN;
      fdset[0].revents = 0;

      int i=1000;

      /* setup tx command */

      struct jdksavdecc_acmpdu cmd;

      cmd.header.cd = 1;
      cmd.header.subtype = JDKSAVDECC_SUBTYPE_ACMP;
      cmd.header.sv = 0;
      cmd.header.version = 0;
      cmd.header.message_type = JDKSAVDECC_ACMP_MESSAGE_TYPE_CONNECT_TX_COMMAND;
      cmd.header.status = JDKSAVDECC_ACMP_STATUS_SUCCESS;
      cmd.header.control_data_length = 44;
      cmd.header.stream_id = f_stream;

      cmd.controller_entity_id = controller_guid;
      cmd.talker_entity_id = remote_talker_guid;
      cmd.listener_entity_id = entity_guid;
      cmd.talker_unique_id = 0;
      cmd.listener_unique_id = 0;
      cmd.stream_dest_mac = f_mac;
      cmd.connection_count = 0;
      cmd.sequence_id = 1;
      cmd.flags = 0;
      cmd.stream_vlan_id = 0;
      cmd.reserved = 0;

      memcpy(buffer, DMAC, 6);
      memcpy(&buffer[6], MAC, 6);
      buffer[12] = 0x22; buffer[13] = 0xf0;

      jdksavdecc_acmpdu_write(&cmd, buffer, 14, sizeof(buffer));

      sendMsg(sock, buffer, 70);

      cmd.header.cd = 1;
      cmd.header.subtype = JDKSAVDECC_SUBTYPE_ACMP;
      cmd.header.sv = 0;
      cmd.header.version = 0;
      cmd.header.message_type = JDKSAVDECC_ACMP_MESSAGE_TYPE_CONNECT_RX_COMMAND;
      cmd.header.status = JDKSAVDECC_ACMP_STATUS_SUCCESS;
      cmd.header.control_data_length = 44;
      cmd.header.stream_id = f_stream;

      cmd.controller_entity_id = controller_guid;
      cmd.talker_entity_id = entity_guid;
      cmd.listener_entity_id = remote_listener_guid;
      cmd.talker_unique_id = 0;
      cmd.listener_unique_id = 0;
      cmd.stream_dest_mac = f_mac;
      cmd.connection_count = 0;
      cmd.sequence_id = 1;
      cmd.flags = 0;
      cmd.stream_vlan_id = 0;
      cmd.reserved = 0;

      memcpy(buffer, DMAC, 6);
      memcpy(&buffer[6], MAC, 6);
      buffer[12] = 0x22; buffer[13] = 0xf0;

      jdksavdecc_acmpdu_write(&cmd, buffer, 14, sizeof(buffer));

      sendMsg(sock, buffer, 70);

      rc = send_ready(ix_stream);

      printf("rc = %d, done sending ready\n", rc);

      rc = mrp_advertise_stream
                    (ox_stream,
		     ox_mac,
                     32 +     /* header size */
                     channels_per_stream *
                     bytes_per_sample *
                     samples_per_interval,
                     1,
                     95, 
                     CLASS_A_VLAN, CLASS_A_PRIORITY);

      printf("rc = %d, advertising stream\n", rc);

      while (1) //poll(&fdset, 1, -1) > 0)
      {

         adpdu.available_index = ++global_available_index;
         
         memcpy(buffer, DMAC, 6);
         memcpy(&buffer[6], MAC, 6);

         buffer[12] = 0x22; buffer[13] = 0xf0;
         jdksavdecc_adpdu_write(&adpdu, &buffer[14], 0, sizeof(buffer));

         sendMsg(sock, buffer, 82);

         sleep(5);

      }
      close(sock);

      rc = mrp_unadvertise_stream
                    (ox_stream,
		     ox_mac,
                     32 +     /* header size */
                     channels_per_stream *
                     bytes_per_sample *
                     samples_per_interval,
                     1,
                     95, 
                     CLASS_A_VLAN, CLASS_A_PRIORITY);

      send_leave(ix_stream);

   }
   return 0;
}
