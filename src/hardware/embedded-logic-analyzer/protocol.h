/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBSIGROK_HARDWARE_EMBEDDED_LOGIC_ANALYZER_PROTOCOL_H
#define LIBSIGROK_HARDWARE_EMBEDDED_LOGIC_ANALYZER_PROTOCOL_H

#include <stdint.h>

#include <string.h>

#include <glib.h>

#include <libsigrok/libsigrok.h>

#include "libsigrok-internal.h"

#include "ela_protocol/ela_protocol.h"

#define LOG_PREFIX "embedded-logic-analyzer"

#define MIN_NUM_SAMPLES 10
#define MIN_SAMPLERATE SR_HZ(100)
#define DEFAULT_SAMPLERATE SR_KHZ(200)
#define DEFAULT_SAMPLE_COUNT 5000
#define DEFAULT_CAPTURE_RATION 10

#define MAX_NUMBER_OF_INPUTS 16

#define NEW_RECEIVE

/* Command opcodes */
// #define CMD_RESET                  0x00
// #define CMD_RUN                    0x01
// #define CMD_ID                     0x02
// #define CMD_TESTMODE               0x03
// #define CMD_METADATA               0x04
// #define CMD_SET_DIVIDER            0x80
// #define CMD_CAPTURE_SIZE           0x81
// #define CMD_SET_FLAGS              0x82
// #define CMD_CAPTURE_DELAYCOUNT     0x83		/* extension for Pepino */
// #define CMD_CAPTURE_READCOUNT      0x84		/* extension for Pepino */
// #define CMD_SET_TRIGGER_MASK       0xc0
// #define CMD_SET_TRIGGER_VALUE      0xc1
// #define CMD_SET_TRIGGER_CONFIG     0xc2

/* Trigger config */
#define TRIGGER_START (1 << 3)

/* Bitmasks for CMD_FLAGS */
/* 12-13 unused, 14-15 RLE mode (we hardcode mode 0). */
#define FLAG_INTERNAL_TEST_MODE (1 << 11)
#define FLAG_EXTERNAL_TEST_MODE (1 << 10)
#define FLAG_SWAP_CHANNELS (1 << 9)
#define FLAG_RLE (1 << 8)
#define FLAG_SLOPE_FALLING (1 << 7)
#define FLAG_CLOCK_EXTERNAL (1 << 6)
#define FLAG_CHANNELGROUP_4 (1 << 5)
#define FLAG_CHANNELGROUP_3 (1 << 4)
#define FLAG_CHANNELGROUP_2 (1 << 3)
#define FLAG_CHANNELGROUP_1 (1 << 2)
#define FLAG_FILTER (1 << 1)
#define FLAG_DEMUX (1 << 0)

typedef enum {
	ELA_REC_STATE_WAITING,
	ELA_REC_STATE_RECEIVING_INFO,
	ELA_REC_STATE_RECEIVING_DATA,
	ELA_REC_STATE_FINISH,
} ela_receive_state;

struct dev_context {
	uint16_t max_channels;
	uint32_t max_samples;
	uint32_t max_samplerate;

	uint64_t cur_samplerate;
	uint64_t limit_samples;
	uint64_t capture_ratio;
	elap_pinmode_t pin_modes[MAX_NUMBER_OF_INPUTS];
	int num_stages;
	int num_of_triggers;

	ela_receive_state receive_state;
	int num_of_retries;
	unsigned int num_of_sample_data;
	unsigned int trigger_sample_index;
	uint8_t *raw_sample_buf;
	unsigned int num_of_bytes;
	uint8_t sampled_info_buf[ELAP_SAMPLED_INFO_SIZE];
};

SR_PRIV extern const char *ela_channel_names[];

SR_PRIV int ela_send_cmd(struct sr_serial_dev_inst *serial, elap_cmd_t command);
SR_PRIV int ela_send_reset(struct sr_serial_dev_inst *serial);
SR_PRIV int ela_send_pinmodes(const struct sr_dev_inst *sdi);
SR_PRIV int ela_convert_pinmodes(const struct sr_dev_inst *sdi);
SR_PRIV struct dev_context *ela_dev_new(void);
SR_PRIV void ela_channel_new(struct sr_dev_inst *sdi, int num_chan);
SR_PRIV int ela_receive_metadata(struct sr_serial_dev_inst *serial, elap_cmd_t *command,
																 GString *devname);
SR_PRIV void ela_abort_acquisition(const struct sr_dev_inst *sdi);
SR_PRIV int ela_receive_data(int fd, int revents, void *cb_data);
SR_PRIV int ela_receive_sampled_data(const struct sr_dev_inst *sdi);

#endif
