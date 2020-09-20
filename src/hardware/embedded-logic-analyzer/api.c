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

#define SERIALCOMM "115200/8n1"

static const uint32_t scanopts[] = {
		SR_CONF_CONN,
		SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
		SR_CONF_LOGIC_ANALYZER,
};

static const uint32_t devopts[] = {
		SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
		SR_CONF_SAMPLERATE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
		SR_CONF_TRIGGER_MATCH | SR_CONF_LIST, SR_CONF_CAPTURE_RATIO | SR_CONF_GET | SR_CONF_SET,
		//	SR_CONF_EXTERNAL_CLOCK | SR_CONF_SET,
		//	SR_CONF_SWAP | SR_CONF_SET,
		//	SR_CONF_RLE | SR_CONF_GET | SR_CONF_SET,
};

static const int32_t trigger_matches[] = {
		SR_TRIGGER_RISING,
		SR_TRIGGER_FALLING,
		SR_TRIGGER_EDGE,
};

/* Channels are numbered 0-31 (on the PCB silkscreen). */
SR_PRIV const char *ela_channel_names[] = {
		"D0",	 "D1",	"D2",	 "D3",	"D4",	 "D5",	"D6",	 "D7",	"D8",	 "D9",	"D10",
		"D11", "D12", "D13", "D14", "D15", "D16", "D17", "D18", "D19", "D20", "D21",
		"D22", "D23", "D24", "D25", "D26", "D27", "D28", "D29", "D30", "D31",
};

#define RESPONSE_DELAY_US (20 * 1000)

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct sr_config *src;
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	GSList *l;
	int ret;
	unsigned int i;
	const char *conn, *serialcomm;
	char buf[ELAP_HANDSHAKE_REPLY_SIZE];
	GString *devname;
	elap_cmd_t command;

	conn = serialcomm = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case SR_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		case SR_CONF_SERIALCOMM:
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (!conn)
		return NULL;

	if (!serialcomm)
		serialcomm = SERIALCOMM;

	serial = sr_serial_dev_inst_new(conn, serialcomm);

	/* The discovery procedure is like this: first send the Reset
	 * command (0x00) 5 times, since the device could be anywhere
	 * in a 5-byte command. Then send the ID command (0x02).
	 * If the device responds with 4 bytes ("OLS1" or "SLA1"), we
	 * have a match.
	 */
	sr_info("Probing %s.", conn);
	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		return NULL;

	if (ela_send_reset(serial) != SR_OK) {
		serial_close(serial);
		sr_err("Could not use port %s. Quitting.", conn);
		return NULL;
	}
	command.type = CMD_HANDSHAKE;
	if (ela_send_cmd(serial, command) != SR_OK) {
		sr_err("Could not send HANDSHAKE command");
		return NULL;
	}

	g_usleep(RESPONSE_DELAY_US);

	if (serial_has_receive_data(serial) == 0) {
		sr_dbg("Didn't get any reply.");
		return NULL;
	}

	ret = serial_read_blocking(serial, buf, ELAP_HANDSHAKE_REPLY_SIZE,
														 serial_timeout(serial, ELAP_HANDSHAKE_REPLY_SIZE));
	if (ret != ELAP_HANDSHAKE_REPLY_SIZE) {
		sr_err("Invalid reply (expected %d bytes, got %d).", ELAP_HANDSHAKE_REPLY_SIZE, ret);
		return NULL;
	}

	if (strncmp(buf, ELAP_HANDSHAKE_REPLY, ELAP_HANDSHAKE_REPLY_SIZE)) {
		sr_err("Invalid reply (expected %s, got "
					 "'%s').",
					 ELAP_HANDSHAKE_REPLY, buf);
		return NULL;
	}

	command.type = CMD_GET;
	command.subtype = SUB_METADATA;
	if (ela_send_cmd(serial, command) != SR_OK) {
		sr_err("Could not send METADATA command");
		return NULL;
	}

	g_usleep(RESPONSE_DELAY_US);
	devname = g_string_new("");
	if (ela_receive_metadata(serial, &command, devname) != SR_OK) {
		g_string_free(devname, TRUE);
		sr_err("Didn't receive metadata");
		return NULL;
	}
	devc = ela_dev_new();
	devc->num_of_triggers = 0;
	devc->max_channels = command.data.metadata.numof_pins;
	devc->max_samples = command.data.metadata.max_sample_cout;
	devc->max_samplerate = command.data.metadata.max_samplerate;

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->priv = devc;
	;
	sdi->model = devname->str;
	g_string_free(devname, FALSE);
	sdi->version = g_strdup("v1.0");

	devc->cur_samplerate = DEFAULT_SAMPLERATE;
	devc->limit_samples = DEFAULT_SAMPLE_COUNT;
	devc->capture_ratio = DEFAULT_CAPTURE_RATION;

	sdi->inst_type = SR_INST_SERIAL;
	sdi->conn = serial;

	for (i = 0; i < devc->max_channels; i++)
		sr_channel_new(sdi, i, SR_CHANNEL_LOGIC, TRUE, ela_channel_names[i]);

	memset(devc->pin_modes, PM_DIGITAL_ON, MAX_NUMBER_OF_INPUTS);
	// for (i = 0; i < MAX_NUMBER_OF_INPUTS; i++) {
	//	devc->pin_modes[i] = PM_DIGITAL_ON;
	//}

	serial_close(serial);

	return std_scan_complete(di, g_slist_append(NULL, sdi));
}

static int config_get(uint32_t key, GVariant **data, const struct sr_dev_inst *sdi,
											const struct sr_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(devc->cur_samplerate);
		break;
	case SR_CONF_CAPTURE_RATIO:
		*data = g_variant_new_uint64(devc->capture_ratio);
		break;
	case SR_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data, const struct sr_dev_inst *sdi,
											const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	uint64_t tmp_u64;

	(void)cg;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_SAMPLERATE:
		tmp_u64 = g_variant_get_uint64(data);
		if (tmp_u64 < MIN_SAMPLERATE || tmp_u64 > devc->max_samplerate)
			return SR_ERR_SAMPLERATE;
		devc->cur_samplerate = tmp_u64;
		break;
	case SR_CONF_LIMIT_SAMPLES:
		tmp_u64 = g_variant_get_uint64(data);
		if (tmp_u64 < MIN_NUM_SAMPLES || tmp_u64 > devc->max_samples)
			return SR_ERR;
		devc->limit_samples = tmp_u64;
		break;
	case SR_CONF_CAPTURE_RATIO:
		devc->capture_ratio = g_variant_get_uint64(data);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data, const struct sr_dev_inst *sdi,
											 const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	uint64_t samplerates[] = {
			MIN_SAMPLERATE,
			0,
			SR_HZ(1),
	};

	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
	case SR_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
	case SR_CONF_SAMPLERATE:
		if (!sdi)
			return SR_ERR_ARG;
		devc = sdi->priv;
		if (devc->max_samplerate == 0)
			return SR_ERR_NA;
		*data = std_gvar_samplerates(samplerates, samplerates_count);
		break;
	case SR_CONF_TRIGGER_MATCH:
		*data = std_gvar_array_i32(ARRAY_AND_SIZE(trigger_matches));
		;
		break;
	case SR_CONF_LIMIT_SAMPLES:
		if (!sdi)
			return SR_ERR_ARG;
		devc = sdi->priv;
		if (devc->max_samples == 0)
			return SR_ERR_NA;
		*data = std_gvar_tuple_u64(MIN_NUM_SAMPLES, devc->max_samples);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	uint32_t pretrig_count;
	elap_cmd_t command;
	devc = sdi->priv;
	serial = sdi->conn;

	pretrig_count = devc->limit_samples * (devc->capture_ratio / 100.0);
	serial_close(serial);
	serial_open(serial, SERIAL_RDWR);
	command.type = CMD_SET;
	command.subtype = SUB_SAMPLERATE;
	command.data.samplerate = devc->cur_samplerate;

	if (ela_send_cmd(serial, command) != SR_OK)
		return SR_ERR;

	command.subtype = SUB_SAMPLE_COUNT;
	command.data.samplerate = devc->limit_samples;

	if (ela_send_cmd(serial, command) != SR_OK)
		return SR_ERR;

	command.subtype = SUB_PRETRIG_COUNT;
	command.data.pretrig_count = pretrig_count;

	if (ela_send_cmd(serial, command) != SR_OK)
		return SR_ERR;

	if (ela_send_pinmodes(sdi) != SR_OK)
		return SR_ERR;

	command.type = CMD_START;

	if (ela_send_cmd(serial, command) != SR_OK)
		return SR_ERR;

	std_session_send_df_header(sdi);

	devc->num_of_retries = 2;
	devc->receive_state = ELA_REC_STATE_WAITING;
	devc->raw_sample_buf = NULL;

	serial_source_add(sdi->session, serial, G_IO_IN, 100,
			ela_receive_data, (struct sr_dev_inst *)sdi);

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	struct sr_serial_dev_inst *serial;
	elap_cmd_t command;

	serial = sdi->conn;

	command.type = CMD_STOP;
	ela_send_cmd(serial, command);
	ela_abort_acquisition(sdi);
	return SR_OK;
}

static struct sr_dev_driver ela_driver_info = {
		.name = "ela",
		.longname = "Embedded logic analyzer",
		.api_version = 1,
		.init = std_init,
		.cleanup = std_cleanup,
		.scan = scan,
		.dev_list = std_dev_list,
		.dev_clear = std_dev_clear,
		.config_get = config_get,
		.config_set = config_set,
		.config_list = config_list,
		.dev_open = std_serial_dev_open,
		.dev_close = std_serial_dev_close,
		.dev_acquisition_start = dev_acquisition_start,
		.dev_acquisition_stop = dev_acquisition_stop,
		.context = NULL,
};
SR_REGISTER_DEV_DRIVER(ela_driver_info);
