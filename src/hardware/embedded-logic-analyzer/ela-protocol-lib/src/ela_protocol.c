#include "ela_protocol.h"

// Declaration of private dunctions ----------
int elap_is_valid_subtype(const elap_cmd_type_t type, const elap_cmd_subtype_t subtype);
int elap_is_valid_type(const elap_cmd_type_t type);
void elap_metadata_to_packet(data_metadata_t* const data, uint8_t* buffer, int* buffer_offset);
int elap_string_size(char* string);
//-------------------------------------------

/**
 * @brief How many bytes are requierd to receive for command of particular type and subtype
 * @param raw_type type of command in bytes (directly from rx buffer)
 * @param raw_subtype subtype of command in bytes (ignored if type doesnt require subtype)
 * @returns number of bytes in command (including type and subtype)
 */
int elap_bytes_in_cmd_raw(const byte_cmd_type raw_type, const byte_cmd_subtype raw_subtype) {
  elap_cmd_type_t type = (elap_cmd_type_t)raw_type;
  elap_cmd_subtype_t subtype = (elap_cmd_subtype_t)raw_subtype;
  if (!elap_is_valid_type(type) ||
      ((elap_has_subtype_raw(type) == 1) && !elap_is_valid_subtype(type, subtype))) {
    return ELAP_RET_FAIL;
  } else {
    return elap_bytes_in_cmd(type, subtype);
  }
}

/**
 * @brief Does a command require a subtype
 * @param raw_type type of command in bytes
 * @returns 1 or 0 if command requires subtype or FAIL if type is invalid
 */
int elap_has_subtype_raw(const byte_cmd_type raw_type) {
  elap_cmd_type_t type = (elap_cmd_type_t)raw_type;
  return elap_has_subtype(type);
}

/**
 * @brief Does a command require a subtype
 * @param type type of command
 * @returns 1 or 0 if command requires subtype or FAIL if type is invalid
 */
int elap_has_subtype(const elap_cmd_type_t type) {
  if (type > CMD_ENUM_SHORT_END && type <= CMD_ENUM_END) {
    return 1;
  } else if (type >= CMD_ENUM_START && type <= CMD_ENUM_SHORT_END) {
    return 0;
#ifdef ELAP_DBG_CMDS
  } else if (type >= CMD_DBG_ENUM_START && type <= CMD_DBG_ENUM_END) {
    return 0;
#endif
  } else {
    return ELAP_RET_FAIL;
  }
}

/**
 * @brief Is type of command valid
 * @param type type of command
 * @returns true if type is valid, false if not
 */
int elap_is_valid_type(const elap_cmd_type_t type) {
  if (type >= CMD_ENUM_START && type <= CMD_ENUM_END) {
    return 1;
#ifdef ELAP_DBG_CMDS
  } else if (type >= CMD_DBG_ENUM_START && type <= CMD_DBG_ENUM_END) {
    return 1;
#endif
  } else {
    return 0;
  }
}

/**
 * @brief Is subtype of command valid
 * @param type type of command
 * @param subtype subtype of command
 * @returns true if type is valid, false if not
 */
int elap_is_valid_subtype(const elap_cmd_type_t type, const elap_cmd_subtype_t subtype) {
  int has_subtype = elap_has_subtype(type);
  if (has_subtype == 0) {
    return 1;
  } else if (has_subtype == ELAP_RET_FAIL) {
    return 0;
  } else if (subtype < SUB_ENUM_START || subtype > SUB_ENUM_END) {
    return 0;
  } else if (type <= CMD_ENUM_SHORT_END) {
    return 1;
  } else if (type == CMD_SET && subtype > SUB_ENUM_SHARED_END) {
    return 0;
  } else {
    return 1;
  }
}

/**
 * @brief Translate command to byte buffer
 * @param[in] cmd pointer to command to translate
 * @param[in] buffer_offset buffer index to start filling from
 * @param[out] buffer byte buffer to fill
 * @returns Index of last buffer item or FAIL if command is invalid
 */
int elap_cmd_to_packet(elap_cmd_t* const cmd, uint8_t* buffer, int buffer_offset) {
  int index = buffer_offset;
  if (!elap_is_valid_type(cmd->type) || !elap_is_valid_subtype(cmd->type, cmd->subtype)) {
    return ELAP_RET_FAIL;
  }
  elap_uint_to_bytes(cmd->type, buffer, sizeof(byte_cmd_type), &index);
  if (elap_has_subtype(cmd->type)) {
    elap_uint_to_bytes(cmd->subtype, buffer, sizeof(byte_cmd_subtype), &index);

    if (cmd->type == CMD_SET || cmd->type == CMD_REPORT) {
      if (cmd->subtype == SUB_PRETRIG_COUNT) {
        elap_uint_to_bytes(cmd->data.pretrig_count, buffer, sizeof(byte_pretrig_count), &index);
      } else if (cmd->subtype == SUB_SAMPLERATE) {
        elap_uint_to_bytes(cmd->data.samplerate, buffer, sizeof(byte_samplerate), &index);
      } else if (cmd->subtype == SUB_SAMPLE_COUNT) {
        elap_uint_to_bytes(cmd->data.sample_cout, buffer, sizeof(byte_sample_count), &index);
      } else if (cmd->subtype == SUB_PIN_MODE) {
        elap_uint_to_bytes(cmd->data.pin_mode.number, buffer, sizeof(byte_pin_number), &index);
        elap_uint_to_bytes(cmd->data.pin_mode.mode, buffer, sizeof(byte_pin_mode), &index);
      } else if (cmd->type == CMD_REPORT && cmd->subtype == SUB_METADATA) {
        elap_metadata_to_packet(&(cmd->data.metadata), buffer, &index);
      } else if (cmd->type == CMD_REPORT && cmd->subtype == SUB_SAMPLED_DATA) {
        elap_uint_to_bytes(cmd->data.sampled_data_info.sampled, buffer, sizeof(byte_numof_sampled),
                           &index);
        elap_uint_to_bytes(cmd->data.sampled_data_info.trigger, buffer, sizeof(byte_trigger_index),
                           &index);
      }
    } else if (cmd->type == CMD_GET) {
      if (cmd->subtype == SUB_PIN_MODE) {
        elap_uint_to_bytes(cmd->data.pin_mode.number, buffer, sizeof(byte_pin_number), &index);
      }
    }
  }
  return index;
}

void elap_metadata_to_packet(data_metadata_t* const data, uint8_t* buffer, int* buffer_offset) {
  int starting_index = *buffer_offset;
  int index = starting_index;
  data->str_size = elap_string_size(data->name);
  elap_uint_to_bytes(data->str_size, buffer, sizeof(byte_metadata_str_size), &index);
  elap_uint_to_bytes(data->max_samplerate, buffer, sizeof(byte_samplerate), &index);
  // elap_uint_to_bytes((byte_metadata_type)MD_MAX_SAMPLE_COUNT, buffer, sizeof(byte_metadata_type),
  // &index);
  elap_uint_to_bytes(data->max_sample_cout, buffer, sizeof(byte_sample_count), &index);
  // elap_uint_to_bytes((byte_metadata_type)MD_NUM_OF_PINS, buffer, sizeof(byte_metadata_type),
  // &index);
  elap_uint_to_bytes(data->numof_pins, buffer, sizeof(byte_pin_number), &index);
  *buffer_offset = index;
  return;
}

int elap_packet_to_metadata(data_metadata_t* data, uint8_t* const buffer, int* index) {
  data->str_size = ELAP_BYTES_TO_UINT_TYPE(byte_metadata_str_size, buffer, index);
  data->max_samplerate = ELAP_BYTES_TO_UINT_TYPE(byte_samplerate, buffer, index);
  data->max_sample_cout = ELAP_BYTES_TO_UINT_TYPE(byte_sample_count, buffer, index);
  data->numof_pins = ELAP_BYTES_TO_UINT_TYPE(byte_pin_number, buffer, index);
  return ELAP_RET_OK;
}

/**
 * @brief Translate byte buffer to command
 * @param[out] cmd pointer to command to translate
 * @param[in] buffer_offset buffer index to start filling from
 * @param[out] buffer byte buffer to translate from
 * @returns Index of last buffer item or FAIL if command is invalid
 */
int elap_packet_to_cmd(elap_cmd_t* cmd, uint8_t* const buffer, int buffer_offset) {
  int index = buffer_offset;
  elap_cmd_type_t temp_type =
      (elap_cmd_type_t)elap_bytes_to_uint(buffer, sizeof(byte_cmd_type), &index);
  if (!elap_is_valid_type(temp_type)) {
    return ELAP_RET_FAIL;
  }

  if (elap_has_subtype(temp_type) == 1) {
    elap_cmd_subtype_t temp_subtype =
        (elap_cmd_subtype_t)elap_bytes_to_uint(buffer, sizeof(byte_cmd_subtype), &index);
    if (!elap_is_valid_subtype(temp_type, temp_subtype)) {
      return ELAP_RET_FAIL;
    }
    elap_cmd_data_t temp_data = cmd->data;
    if (temp_type == CMD_SET || temp_type == CMD_REPORT) {
      if (temp_subtype == SUB_PRETRIG_COUNT) {
        temp_data.pretrig_count = ELAP_BYTES_TO_UINT_TYPE(byte_pretrig_count, buffer, &index);
      } else if (temp_subtype == SUB_SAMPLERATE) {
        temp_data.samplerate = ELAP_BYTES_TO_UINT_TYPE(byte_samplerate, buffer, &index);
      } else if (temp_subtype == SUB_SAMPLE_COUNT) {
        temp_data.sample_cout = ELAP_BYTES_TO_UINT_TYPE(byte_sample_count, buffer, &index);
      } else if (temp_subtype == SUB_PIN_MODE) {
        temp_data.pin_mode.number = ELAP_BYTES_TO_UINT_TYPE(byte_pin_number, buffer, &index);
        temp_data.pin_mode.mode =
            (elap_pinmode_t)elap_bytes_to_uint(buffer, sizeof(byte_pin_mode), &index);
      } else if (temp_type == CMD_REPORT && temp_subtype == SUB_METADATA) {
        elap_packet_to_metadata(&(temp_data.metadata), buffer, &index);
      } else if (temp_type == CMD_REPORT && temp_subtype == SUB_SAMPLED_DATA) {
        temp_data.sampled_data_info.sampled =
            ELAP_BYTES_TO_UINT_TYPE(byte_numof_sampled, buffer, &index);
        temp_data.sampled_data_info.trigger =
            ELAP_BYTES_TO_UINT_TYPE(byte_trigger_index, buffer, &index);
      }
    } else if (temp_type == CMD_GET) {
      if (temp_subtype == SUB_PIN_MODE) {
        temp_data.pin_mode.number = ELAP_BYTES_TO_UINT_TYPE(byte_pin_number, buffer, &index);
      }
    } else {
      return ELAP_RET_FAIL;
    }
    cmd->type = temp_type;
    cmd->subtype = temp_subtype;
    cmd->data = temp_data;
  } else if (elap_has_subtype(temp_type) == 0) {
    cmd->type = temp_type;
  } else {
    return ELAP_RET_FAIL;
  }

  return index;
}

/**
 * @brief How many bytes are requierd for command of particular type and subtype
 * @param type type of command
 * @param subtype subtype of command
 * @returns number of bytes in command (including type and subtype)
 */
int elap_bytes_in_cmd(const elap_cmd_type_t type, const elap_cmd_subtype_t subtype) {
  if (type == CMD_RESET || type == CMD_HANDSHAKE || type == CMD_START || type == CMD_STOP) {
    return 0;
  } else if (type == CMD_SET || type == CMD_REPORT) {
    if (subtype == SUB_SAMPLERATE || subtype == SUB_SAMPLE_COUNT || subtype == SUB_PRETRIG_COUNT ||
        subtype == SUB_PIN_MODE) {
      return sizeof(byte_subtype_data);
    } else if (type == CMD_REPORT && subtype == SUB_METADATA) {
      return ELAP_METADATA_SIZE - (sizeof(byte_cmd_type) + sizeof(byte_cmd_subtype));
    } else if (type == CMD_REPORT && subtype == SUB_SAMPLED_DATA) {
      return sizeof(byte_numof_sampled) + sizeof(byte_trigger_index);
    }
  } else if (type == CMD_GET) {
    if (subtype == SUB_SAMPLERATE || subtype == SUB_SAMPLE_COUNT || subtype == SUB_PRETRIG_COUNT ||
        subtype == SUB_METADATA || subtype == SUB_SAMPLED_DATA) {
      return 0;
    } else if (subtype == SUB_PIN_MODE) {
      return sizeof(byte_pin_number);
    }
  }
  return ELAP_RET_FAIL;
}

/**
 * @brief Translate unsigned integer to byte buffer
 * @param num unsigned integer to translate
 * @param buffer pointer of buffer to fill
 * @param num_of_bytes number of bytes required
 * @param buffer_offset pointer to index to fill buffer from, increments by num_of_bytes
 * @returns None
 */
void elap_uint_to_bytes(const uint64_t num, uint8_t* buffer, const int num_of_bytes,
                        int* buffer_offset) {
  for (int i = 0; i < num_of_bytes; i++) {
#ifdef ELAP_LITTLE_EDIAN
    buffer[*buffer_offset + i] = (num >> (i * 8)) & 0xFFU;
#else
    buffer[*buffer_offset + i] = (num >> ((num_of_bytes - 1 - i) * 8)) & 0xFFU;
#endif
  }
  *buffer_offset += num_of_bytes;
  return;
}

/**
 * @brief Translate C string to byte buffer
 * @param string pointer to string to translate, string has to end with '/0'
 * @param buffer pointer of buffer to fill
 * @param buffer_offset pointer to index to fill buffer from, increments by num_of_bytes
 * @returns None
 */
void elap_string_to_bytes(char* const string, uint8_t* buffer, int* buffer_offset) {
  int index = *buffer_offset;
  int i = 0;
  while (string[i] != '\0' && i < (ELAP_NAME_MAX_LEN - 1)) {
    buffer[index] = (uint8_t)string[i];
    i++;
    index++;
  }
  buffer[index] = (uint8_t)string[i];
  *buffer_offset = index;
  return;
}

/**
 * @brief Translate bytes to C style string
 * @param string pointer to a C style string that will end with '/0'
 * @param buffer pointer of buffer to translate from
 * @param buffer_offset pointer to index to fill buffer from, increments by num_of_bytes
 * @returns None
 */
void elap_bytes_to_string(char* string, uint8_t* buffer, int* buffer_offset) {
  int index = *buffer_offset;
  int i = 0;
  string[i] = (char)buffer[index];
  while (string[i] != '\0' && i < ELAP_NAME_MAX_LEN) {
    i++;
    index++;
    string[i] = (char)buffer[index];
  }
  return;
}

/**
 * @brief Size of a C string
 * @param string pointer to a C style string that ends with '/0'
 * @returns Number of symbols in string
 */
int elap_string_size(char* string) {
  int i = 0;
  while (string[i] != '\0') {
    i++;
  }
  return i;
}

/**
 * @brief Translate byte buffer to unsigned integer
 * @param buffer pointer to buffer containing the integer
 * @param num_of_bytes number of bytes required
 * @param buffer_offset pointer to index to fill buffer from, increments by num_of_bytes
 * @returns Translated unsigned integer
 */
uint64_t elap_bytes_to_uint(uint8_t* const buffer, const int num_of_bytes, int* buffer_offset) {
  uint64_t num = 0;
  for (int i = 0; i < num_of_bytes; i++) {
#ifdef ELAP_LITTLE_EDIAN
    num |= buffer[buffer_offset + i] << (i * 8);
#else
    num |= buffer[*buffer_offset + i] << ((num_of_bytes - 1 - i) * 8);
#endif
  }
  *buffer_offset += num_of_bytes;
  return num;
}