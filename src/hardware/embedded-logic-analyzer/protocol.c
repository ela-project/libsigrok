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

#include <config.h>

#include "protocol.h"

SR_PRIV const uint64_t ela_samplerates[] = {
	100,
	200,
	500,
	SR_KHZ(1),
	SR_KHZ(2),
	SR_KHZ(5),
	SR_KHZ(10),
	SR_KHZ(20),
	SR_KHZ(50),
	SR_KHZ(100),
	SR_KHZ(200),
	SR_KHZ(500),
	SR_MHZ(1),
	SR_MHZ(2),
	SR_MHZ(4),
	SR_MHZ(6),
	SR_MHZ(9),
	SR_MHZ(12),
};

SR_PRIV const size_t ela_samplerates_count = ARRAY_SIZE(ela_samplerates);

SR_PRIV int ela_send_cmd(struct sr_serial_dev_inst *serial, elap_cmd_t command)
{
	uint8_t buf[ELAP_CMD_MAX_SIZE];
	int index;
	sr_dbg("Sending cmd type 0x%.4x.", command.type);
	index = elap_cmd_to_packet(&command, buf, 0);
	if (index == ELAP_RET_FAIL)
		return SR_ERR;

	if (serial_write_blocking(serial, buf, index, serial_timeout(serial, index)) != index)
		return SR_ERR;

	if (serial_drain(serial) != 0)
		return SR_ERR;

	return SR_OK;
}

SR_PRIV int ela_send_reset(struct sr_serial_dev_inst *serial)
{
	elap_cmd_t command;
	unsigned int i;

	command.type = CMD_RESET;
	for (i = 0; i < 5; i++) {
		if (ela_send_cmd(serial, command) != SR_OK)
			return SR_ERR;
	}

	return SR_OK;
}

SR_PRIV int ela_send_pinmodes(const struct sr_dev_inst *sdi)
{
	unsigned int i;
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	elap_cmd_t command;

	devc = sdi->priv;
	serial = sdi->conn;

	ela_convert_pinmodes(sdi);

	command.type = CMD_SET;
	command.subtype = SUB_PIN_MODE;
	for (i = 0; i < devc->max_channels; i++) {
		command.data.pin_mode.number = i;
		command.data.pin_mode.mode = devc->pin_modes[i];
		if (ela_send_cmd(serial, command) != SR_OK) {
			return SR_ERR;
		}
	}

	return SR_OK;
}

SR_PRIV int ela_convert_pinmodes(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_trigger *trigger;
	struct sr_trigger_stage *stage;
	struct sr_trigger_match *match;
	struct sr_channel *ch;
	const GSList *l, *m;

	devc = sdi->priv;

	for (l = sdi->channels; l; l = l->next) {
		ch = l->data;
		if (ch->enabled)
			devc->pin_modes[ch->index] = PM_DIGITAL_ON;
		else
			devc->pin_modes[ch->index] = PM_DIGITAL_OFF;
	}

	devc->num_of_triggers = 0;

	if (!(trigger = sr_session_trigger_get(sdi->session)))
		return SR_OK;

	for (l = trigger->stages; l; l = l->next) {
		stage = l->data;
		for (m = stage->matches; m; m = m->next) {
			match = m->data;
			if (!match->channel->enabled) {
				devc->pin_modes[match->channel->index] = PM_DIGITAL_OFF;
			} else if (match->match == SR_TRIGGER_EDGE) {
				devc->pin_modes[match->channel->index] = PM_TRIGGER_BOTH;
				devc->num_of_triggers++;
			} else if (match->match == SR_TRIGGER_RISING) {
				devc->pin_modes[match->channel->index] = PM_TRIGGER_RISING;
				devc->num_of_triggers++;
			} else if (match->match == SR_TRIGGER_FALLING) {
				devc->pin_modes[match->channel->index] = PM_TRIGGER_FALLING;
				devc->num_of_triggers++;
			} else {
				devc->pin_modes[match->channel->index] = PM_DIGITAL_ON;
			}
		}
	}
	return SR_OK;
}

SR_PRIV struct dev_context *ela_dev_new(void)
{
	struct dev_context *devc;

	devc = g_malloc0(sizeof(struct dev_context));

	/* Device-specific settings */
	devc->max_samples = devc->max_samplerate = 0;

	/* Acquisition settings */
	devc->limit_samples = devc->capture_ratio = 0;

	return devc;
}

SR_PRIV void ela_channel_new(struct sr_dev_inst *sdi, int num_chan)
{
	struct dev_context *devc = sdi->priv;
	int i;

	for (i = 0; i < num_chan; i++)
		sr_channel_new(sdi, i, SR_CHANNEL_LOGIC, TRUE, ela_channel_names[i]);

	devc->max_channels = num_chan;
}

SR_PRIV int ela_receive_metadata(struct sr_serial_dev_inst *serial, elap_cmd_t *command,
																 GString *devname)
{
	uint8_t buf[ELAP_METADATA_SIZE];
	elap_cmd_t tmp_command;
	unsigned int i;
	guchar tmp_c;

	if (serial_read_blocking(serial, buf, ELAP_METADATA_SIZE,
													 serial_timeout(serial, ELAP_METADATA_SIZE)) != ELAP_METADATA_SIZE)
		return SR_ERR;

	if (elap_packet_to_cmd(&tmp_command, buf, 0) == ELAP_RET_FAIL)
		return SR_ERR;

	if (tmp_command.type != CMD_REPORT && tmp_command.subtype != SUB_METADATA)
		return SR_ERR;

	for (i = 0; i < tmp_command.data.metadata.str_size; i++) {
		if (serial_read_blocking(serial, &tmp_c, 1, serial_timeout(serial, 1)) != 1)
			return SR_ERR;
		g_string_append_c(devname, tmp_c);
	}

	command->type = tmp_command.type;
	command->subtype = tmp_command.subtype;
	command->data = tmp_command.data;
	return SR_OK;
}

SR_PRIV void ela_abort_acquisition(const struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst *serial;

	serial = sdi->conn;

	serial_source_remove(sdi->session, serial);

	std_session_send_df_end(sdi);
}

#ifdef NEW_RECEIVE
SR_PRIV int ela_receive_sampled_data(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_logic logic;
	int timeout;

	serial = sdi->conn;
	devc = sdi->priv;

	timeout = serial_timeout(serial, devc->num_of_sample_data);
	if (serial_read_blocking(serial, devc->raw_sample_buf, devc->num_of_sample_data,
													 (timeout < 0 || (unsigned int)timeout !=
			devc->num_of_sample_data))) {
		sr_dbg("Error, sampled data");
		return SR_ERR;
	}

	if (devc->num_of_triggers > 0) {
		if (devc->trigger_sample_index > 0) {
			packet.type = SR_DF_LOGIC;
			packet.payload = &logic;
			logic.length = devc->trigger_sample_index;
			logic.unitsize = 1;
			logic.data = devc->raw_sample_buf;
			sr_session_send(sdi, &packet);
		}
		packet.type = SR_DF_TRIGGER;
		sr_session_send(sdi, &packet);

		packet.type = SR_DF_LOGIC;
		packet.payload = &logic;
		logic.length = devc->num_of_sample_data - devc->trigger_sample_index;
		logic.unitsize = 1;
		logic.data = devc->raw_sample_buf + devc->trigger_sample_index;
		sr_session_send(sdi, &packet);

	} else {
		packet.type = SR_DF_LOGIC;
		packet.payload = &logic;
		logic.length = devc->num_of_sample_data;
		logic.unitsize = 1;
		logic.data = devc->raw_sample_buf;
		sr_session_send(sdi, &packet);
	}

	std_session_send_df_end(sdi);
	g_free(devc->raw_sample_buf);
	return SR_OK;
}

SR_PRIV int ela_receive_data(int fd, int revents, void *cb_data)
{
	struct dev_context *devc;
	struct sr_dev_inst *sdi;
	struct sr_serial_dev_inst *serial;
	elap_cmd_t command;
	uint8_t byte;

	(void)fd;

	sdi = cb_data;
	serial = sdi->conn;
	devc = sdi->priv;

	if (devc->receive_state == ELA_REC_STATE_WAITING) {
		if (revents == 0) {
			return TRUE;
		} else if (revents == G_IO_IN) {
			devc->num_of_bytes = 0;
			devc->receive_state = ELA_REC_STATE_RECEIVING_INFO;
			if (serial_read_nonblocking(serial, &byte, 1) != 1) {
				sr_dbg("Error, receiving info");
				return FALSE;
			}
			devc->sampled_info_buf[devc->num_of_bytes] = byte;
			devc->num_of_bytes++;
		}
	} else if (devc->receive_state == ELA_REC_STATE_RECEIVING_INFO) {
		if (revents == G_IO_IN) {
			if (serial_read_nonblocking(serial, &byte, 1) != 1) {
				sr_dbg("Error, receiving info");
				return FALSE;
			}
			devc->sampled_info_buf[devc->num_of_bytes] = byte;
			devc->num_of_bytes++;
			if (devc->num_of_bytes == ELAP_SAMPLED_INFO_SIZE) {
				if (elap_packet_to_cmd(&command, devc->sampled_info_buf, 0) == ELAP_RET_FAIL) {
					sr_dbg("Error, translating command");
					return FALSE;
				} else if (command.type != CMD_REPORT && command.subtype != SUB_SAMPLED_DATA) {
					sr_dbg("Error, command invalid");
					return FALSE;
				}

				devc->num_of_sample_data = command.data.sampled_data_info.sampled;
				devc->trigger_sample_index = command.data.sampled_data_info.trigger;
				sr_dbg("Received sampled data info: ammount %d, trigger index %d", devc->num_of_sample_data,
							 devc->trigger_sample_index);
				devc->num_of_bytes = 0;
				devc->receive_state = ELA_REC_STATE_RECEIVING_DATA;
				devc->raw_sample_buf = g_try_malloc(devc->num_of_sample_data);
				if (!devc->raw_sample_buf) {
					sr_err("Sample buffer malloc failed.");
					return FALSE;
				}
				serial_source_remove(sdi->session, serial);
				ela_receive_sampled_data(sdi);
			}
		} else {
			sr_dbg("Timeout ELA_REC_STATE_RECEIVING_INFO");
			return FALSE;
		}
	}

	return TRUE;
}
#endif

#ifndef NEW_RECEIVE
SR_PRIV int ela_receive_data(int fd, int revents, void *cb_data)
{
	struct dev_context *devc;
	struct sr_dev_inst *sdi;
	struct sr_serial_dev_inst *serial;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_logic logic;
	elap_cmd_t command;
	uint8_t byte;

	(void)fd;

	sdi = cb_data;
	serial = sdi->conn;
	devc = sdi->priv;

	if (devc->receive_state == ELA_REC_STATE_WAITING) {
		if (revents == 0) {
			return TRUE;
		} else if (revents == G_IO_IN) {
			devc->num_of_bytes = 0;
			devc->receive_state = ELA_REC_STATE_RECEIVING_INFO;
			if (serial_read_nonblocking(serial, &byte, 1) != 1) {
				sr_dbg("Error, receiving info");
				return FALSE;
			}
			devc->sampled_info_buf[devc->num_of_bytes] = byte;
			devc->num_of_bytes++;
		}
	} else if (devc->receive_state == ELA_REC_STATE_RECEIVING_INFO) {
		if (revents == G_IO_IN) {
			if (serial_read_nonblocking(serial, &byte, 1) != 1) {
				sr_dbg("Error, receiving info");
				return FALSE;
			}
			devc->sampled_info_buf[devc->num_of_bytes] = byte;
			devc->num_of_bytes++;
			if (devc->num_of_bytes == ELAP_SAMPLED_INFO_SIZE) {
				if (elap_packet_to_cmd(&command, devc->sampled_info_buf, 0) == ELAP_RET_FAIL) {
					sr_dbg("Error, translating command");
					return FALSE;
				} else if (command.type != CMD_REPORT && command.subtype != SUB_SAMPLED_DATA) {
					sr_dbg("Error, command invalid");
					return FALSE;
				}

				devc->num_of_sample_data = command.data.sampled_data_info.sampled;
				devc->trigger_sample_index = command.data.sampled_data_info.trigger;
				sr_dbg("Received sampled data info: ammount %d, trigger index %d", devc->num_of_sample_data,
							 devc->trigger_sample_index);
				devc->num_of_bytes = 0;
				devc->receive_state = ELA_REC_STATE_RECEIVING_DATA;
				devc->raw_sample_buf = g_try_malloc(devc->num_of_sample_data);
				if (!devc->raw_sample_buf) {
					sr_err("Sample buffer malloc failed.");
					return FALSE;
				}
			}
		} else {
			sr_dbg("Timeout ELA_REC_STATE_RECEIVING_INFO");
			return FALSE;
		}
	} else if (devc->receive_state == ELA_REC_STATE_RECEIVING_DATA) {
		if (revents == G_IO_IN) {
			if (serial_read_blocking(serial, &byte, 1, serial_timeout(serial, 1)) != 1) {
				sr_dbg("Data badly received: index %d", devc->num_of_bytes);
				g_free(devc->raw_sample_buf);
			 	return FALSE;
			} else {
				sr_spew("Received byte 0x%.2x., index %d", byte, devc->num_of_bytes);
				devc->raw_sample_buf[devc->num_of_bytes] = byte;
				devc->num_of_bytes++;
				if (devc->num_of_bytes >= devc->num_of_sample_data) {
					devc->receive_state = ELA_REC_STATE_FINISH;
				}
			}
		} else {
			g_free(devc->raw_sample_buf);
			sr_dbg("Timeout ELA_REC_STATE_RECEIVING_DATA");
			return FALSE;
		}
	}
	if (devc->receive_state == ELA_REC_STATE_FINISH) {
		if (devc->num_of_triggers > 0) {
			if (devc->trigger_sample_index > 0) {
				packet.type = SR_DF_LOGIC;
				packet.payload = &logic;
				logic.length = devc->trigger_sample_index;
				logic.unitsize = 1;
				logic.data = devc->raw_sample_buf;
				sr_session_send(sdi, &packet);
			}
			packet.type = SR_DF_TRIGGER;
			sr_session_send(sdi, &packet);

			if(devc->num_of_sample_data > devc->trigger_sample_index){
				packet.type = SR_DF_LOGIC;
				packet.payload = &logic;
				logic.length = devc->num_of_sample_data - devc->trigger_sample_index;
				logic.unitsize = 1;
				logic.data = devc->raw_sample_buf + devc->trigger_sample_index;
			}
			sr_session_send(sdi, &packet);

		} else {
			packet.type = SR_DF_LOGIC;
			packet.payload = &logic;
			logic.length = devc->num_of_sample_data;
			logic.unitsize = 1;
			logic.data = devc->raw_sample_buf;
			sr_session_send(sdi, &packet);
		}
		g_free(devc->raw_sample_buf);

		serial_flush(serial);
		ela_abort_acquisition(sdi);
	}

	return TRUE;
}
#endif
