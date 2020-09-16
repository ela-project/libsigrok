#ifndef ELA_PROTOCOL_H
#define ELA_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define ELAP_HANDSHAKE_REPLY "ELAPV1"
#define ELAP_HANDSHAKE_REPLY_SIZE 7

#define ELAP_NAME_MAX_LEN 20

#define ELAP_SAMPLED_INFO_SIZE                                                     \
  (sizeof(byte_cmd_type) + sizeof(byte_cmd_subtype) + sizeof(byte_numof_sampled) + \
   sizeof(byte_trigger_index))

#define ELAP_METADATA_SIZE                                                             \
  (sizeof(byte_cmd_type) + sizeof(byte_cmd_subtype) + sizeof(byte_metadata_str_size) + \
   sizeof(byte_samplerate) + sizeof(byte_sample_count) + sizeof(byte_pin_number))

#define ELAP_CMD_MAX_SIZE ELAP_METADATA_SIZE

#define ELAP_CMD_TYPE_SIZE ((int)sizeof(byte_cmd_type))
#define ELAP_CMD_SUBTYPE_SIZE ((int)sizeof(byte_cmd_subtype))

#define ELAP_BIG_EDIAN

#ifndef ELAP_BIG_EDIAN
#define ELAP_LITTLE_EDIAN
#endif

#define ELAP_RET_FAIL -1
#define ELAP_RET_OK 0

typedef enum {
  CMD_ENUM_START = 0x00U,

  // Short commands
  CMD_RESET = CMD_ENUM_START,
  CMD_HANDSHAKE,
  CMD_START,
  CMD_STOP,
  CMD_ENUM_SHORT_END = CMD_STOP,

  // Long commands
  CMD_SET,
  CMD_GET,
  CMD_REPORT,

  CMD_ENUM_END = CMD_REPORT,
#ifdef ELAP_DBG_CMDS
  CMD_DBG_ENUM_START,
  CMD_DBG = CMD_DBG_ENUM_START,
  CMD_DBG_ENUM_END = CMD_DBG,
#endif
} elap_cmd_type_t;

typedef enum {
  SUB_ENUM_START = 0x01U,

  // SET, GET, REPORT
  SUB_SAMPLERATE = SUB_ENUM_START,
  SUB_SAMPLE_COUNT,
  SUB_PRETRIG_COUNT,
  SUB_PIN_MODE,
  SUB_ENUM_SHARED_END = SUB_PIN_MODE,

  // GET, REPORT only
  SUB_METADATA,
  SUB_SAMPLED_DATA,

  SUB_ENUM_END = SUB_SAMPLED_DATA,
} elap_cmd_subtype_t;

typedef enum {
  PM_INVALID = 0x00U,
  PM_ENUM_START = 0x01U,

  PM_DIGITAL_OFF = PM_ENUM_START,
  PM_DIGITAL_ON,
  PM_TRIGGER_BEGIN,
  PM_TRIGGER_HIGH = PM_TRIGGER_BEGIN,
  PM_TRIGGER_LOW,
  PM_TRIGGER_RISING,
  PM_TRIGGER_FALLING,
  PM_TRIGGER_BOTH,

  PM_ENUM_END = PM_TRIGGER_BOTH,
} elap_pinmode_t;

/*typedef enum {
        MD_ENUM_START = 0x01U,

        MD_MAX_SAMPLERATE = MD_ENUM_START,
        MD_MAX_SAMPLE_COUNT,
        MD_NUM_OF_PINS,
        MD_NAME,

        MD_ENUM_END,
} elap_metadata_type_t;*/

typedef uint8_t byte_cmd_type;
typedef uint8_t byte_cmd_subtype;
// typedef uint8_t byte_metadata_type;
typedef uint8_t byte_metadata_str_size;
typedef uint32_t byte_subtype_data;
typedef uint16_t byte_pin_number;
typedef uint16_t byte_pin_mode;
typedef uint32_t byte_samplerate;
typedef uint32_t byte_pretrig_count;
typedef uint32_t byte_sample_count;
typedef uint32_t byte_numof_sampled;
typedef uint32_t byte_trigger_index;

typedef struct {
  byte_numof_sampled sampled;
  byte_trigger_index trigger;
} data_sampled_data_info_t;

typedef struct {
  byte_pin_number number;
  elap_pinmode_t mode;
} data_pimode_t;

typedef struct {
  byte_metadata_str_size str_size;
  byte_samplerate max_samplerate;
  byte_sample_count max_sample_cout;
  byte_pin_number numof_pins;
  char *name;
} data_metadata_t;

typedef union {
  byte_samplerate samplerate;
  byte_sample_count sample_cout;
  byte_pretrig_count pretrig_count;
  data_pimode_t pin_mode;
  data_sampled_data_info_t sampled_data_info;
  data_metadata_t metadata;
} elap_cmd_data_t;

typedef struct {
  elap_cmd_type_t type;
  elap_cmd_subtype_t subtype;
  elap_cmd_data_t data;
} elap_cmd_t;

#define ELAP_BYTES_TO_UINT_TYPE(type, buffer_ptr, index_ptr) \
  ((type)elap_bytes_to_uint(buffer_ptr, sizeof(type), index_ptr))

void elap_uint_to_bytes(const uint64_t num, uint8_t *buffer, const int num_of_bytes,
                        int *buffer_offset);
uint64_t elap_bytes_to_uint(uint8_t *const buffer, const int num_of_bytes, int *buffer_offset);

int elap_cmd_to_packet(elap_cmd_t *const cmd, uint8_t *buffer, int buffer_offset);
int elap_packet_to_cmd(elap_cmd_t *cmd, uint8_t *const buffer, int buffer_offset);
int elap_has_subtype(const elap_cmd_type_t type);
int elap_bytes_in_cmd(const elap_cmd_type_t type, const elap_cmd_subtype_t sg_type);
int elap_bytes_in_cmd_raw(const byte_cmd_type raw_type, const byte_cmd_subtype raw_subtype);
int elap_has_subtype_raw(const byte_cmd_type raw_type);
int elap_packet_to_metadata(data_metadata_t *data, uint8_t *const buffer, int *index);
void elap_bytes_to_string(char *string, uint8_t *buffer, int *buffer_offset);
void elap_string_to_bytes(char *const string, uint8_t *buffer, int *buffer_offset);

#ifdef __cplusplus
}
#endif

#endif