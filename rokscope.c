#include "rokscope.h"


void assert_sr(int ret, const char *string) {
	if (ret != SR_OK) {
		fprintf(stderr, "Error %s (%s): %s.\n", string, sr_strerror_name(ret),
				sr_strerror(ret));
		exit(1);
	}
}


const char *configkey_tostring(uint16_t option) {
	const char *option_name = NULL;
	switch (option) {
		case 10000: option_name = "Logic Analyzer"; break;
		case 10001: option_name = "Oscilloscope"; break;
		case 20000: option_name = "Connection"; break;
		case 30000: option_name = "Sample Rate"; break;
		case 30012: option_name = "Volts/Div"; break;
		case 30013: option_name = "Coupling"; break;
		case 30017: option_name = "Number of Vertical Divisions"; break;
		case 50000: option_name = "Sample Time Limit (ms)"; break;
		case 50001: option_name = "Sample Number Limit"; break;
	}
	return option_name;
}


void enumerate_device_options(const char *name, struct sr_dev_driver *driver,
		struct sr_dev_inst *dev, struct sr_channel_group *chgroup) {
	GArray *options_list = sr_dev_options(driver, dev, chgroup);
	if (options_list == NULL) {
		fprintf(stderr, "Error getting options list from %s!\n", name);
		exit(1);
		return;
	}

	for (guint i = 0; i < options_list->len; i++) {
		uint32_t option = g_array_index(options_list, uint32_t, i);
		const char *option_name = configkey_tostring(option);

		if (option_name == NULL) {
			printf("%s option %d available\n", name, option);
		} else {
			printf("%s option %d available: %s\n", name, option, option_name);
		}
	}
	g_array_free(options_list, TRUE);
}


struct sr_channel_group* get_device_channel_group(struct sr_dev_inst *dev) {
	struct sr_channel_group *chgroup;
	GSList *chg_list = sr_dev_inst_channel_groups_get(dev);
	if (chg_list == NULL) {
		fprintf(stderr, "Error getting channel groups list from device!\n");
		exit(1);
		return NULL;
	}
	chgroup = chg_list->data;
	return chgroup;
}


struct sr_channel** get_device_channels(struct sr_dev_inst *dev) {
	GSList *ch_list;
	struct sr_channel **channels;
	if ((ch_list = sr_dev_inst_channels_get(dev)) == NULL) {
		fprintf(stderr, "Error enumerating channels\n");
		exit(1);
	}
	guint ch_count = g_slist_length(ch_list);		
	channels = malloc((1 + ch_count) * sizeof(struct sr_channel *));
	if (channels == NULL) {
		perror("malloc");
		exit(1);
	}
	for (guint i = 0; i < ch_count && ch_list != NULL; ch_list = ch_list->next) {
		struct sr_channel *channel;
		channel = ch_list->data;
		channels[i++] = channel;
		assert_sr( sr_dev_channel_enable(channel, TRUE), "enabling channel");
	}
	channels[ch_count] = NULL;
	return channels;
}


struct sr_dev_driver *get_hantek_6xxx_driver(struct sr_context *sr_ctx) {
	struct sr_dev_driver** drivers;
	struct sr_dev_driver* driver = NULL;
	drivers = sr_driver_list(sr_ctx);
	if (drivers == NULL) {
		fprintf(stderr, "No drivers found!\n");
		exit(1);
		return NULL;
	}

	for (int i = 0; drivers[i] != NULL; i++) {
		driver = drivers[i];
		if (0 == strcmp(driver->name, "hantek-6xxx")) {
			int r = sr_driver_init(sr_ctx, driver);
			if (r != SR_OK) {
				fprintf(stderr, "Error initializing driver!\n");
				exit(1);
				return NULL;
			}
			
			return driver;
		}
	}
	fprintf(stderr, "hantek_6xxx driver not found!\n");
	exit(1);
	return NULL;
}


struct sr_dev_inst *get_hantek_6xxx_device(struct sr_dev_driver *driver) {
	GSList* dev_list = sr_driver_scan(driver, NULL);
	if (dev_list == NULL) {
		fprintf(stderr, "No devices found\n");
		exit(1);
		return NULL;
	}

	struct sr_dev_inst *dev;
	dev = dev_list->data;

	g_slist_free(dev_list);
	return dev;
}


void on_session_stopped(void *data) {
	struct state *s = data;
	s->buff_idx++;
	if (s->running)
		assert_sr( sr_session_start(s->session), "starting session");
}


void on_session_datafeed(const struct sr_dev_inst *dev,
		const struct sr_datafeed_packet *packet, void *data) {
	UNUSED(data);
	UNUSED(dev);
	uint16_t type = packet->type;
	struct state *s = data;

	switch (type) {

		case SR_DF_HEADER: {
			const struct sr_datafeed_header *payload;
			payload = packet->payload;
			UNUSED(payload);
		} break;

		case SR_DF_END: {
		} break;

		case SR_DF_ANALOG: {
			const struct sr_datafeed_analog *payload;
			payload = packet->payload;
			float *payload_data = payload->data;

			GLfloat *vert_data = s->gloscope->vert_data;
			int size = payload->num_samples;
			if (size > s->gloscope->num_samples)
				size = s->gloscope->num_samples;
			for (int i = 0; i < size; i++) {
				vert_data[i] = payload_data[i];
			}
		} break;

		default:
			printf("unknown datafeed type %d\n", type);

	}
}


void stdin_data(struct state *s, const char *stdin_buff, gssize bytes_read) {
	char *cmdend = memchr(stdin_buff, '\n', bytes_read);

	if (cmdend != NULL) {
		cmdend[0] = 0;
		char *str = malloc(STDIN_BUFF_SIZE);
		memcpy(str, stdin_buff, STDIN_BUFF_SIZE);
		parse_command(s, str);
		free(str);
	}
}


void on_stdin_read(GObject *src, GAsyncResult *res, void *data) {
	struct state *s = data;
	GInputStream *input = (GInputStream *) src;
	GError *error;
	gssize bytes_read;
	bytes_read = g_input_stream_read_finish(input, res, &error);
	if (bytes_read == -1) {
		printf("Error in stdin: %s\n.", error->message);
		exit(1);
		return;
	} else if (bytes_read == 0) {
		printf("Stdin finished.\n");
	} else {
		stdin_data(s, s->stdin_buff, bytes_read);

		g_input_stream_read_async(input, s->stdin_buff, STDIN_BUFF_SIZE,
				G_PRIORITY_DEFAULT, NULL, on_stdin_read, s);
	}
}


void *start_render_thread(void *data) {
	struct state *s = data;
	
	struct gloscope_context ctx;
	gloscope_init(&ctx);
	s->gloscope = &ctx;
	int e;
	do {
		e = gloscope_render(&ctx);
	} while (!e);

	exit(0);
	return NULL;
}


int main(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);

	struct state s;
	int ret;
	s.buff_idx = 0;
	s.running = FALSE;

	GMainLoop *main_loop;
	main_loop = g_main_loop_new(NULL, FALSE);

	s.rthread = g_thread_new("renderer", start_render_thread, &s);

	GInputStream *input;
	input = g_unix_input_stream_new(0, FALSE);
	g_input_stream_read_async(input, s.stdin_buff, 80, G_PRIORITY_DEFAULT,
			NULL, on_stdin_read, &s);

	s.context = NULL;
	assert_sr( sr_init(&s.context), "initializing libsigrok");

	s.driver = get_hantek_6xxx_driver(s.context);
	enumerate_device_options("Driver", s.driver, NULL, NULL);

	s.device = get_hantek_6xxx_device(s.driver);
	enumerate_device_options("Device", s.driver, s.device, NULL);
	assert_sr( sr_dev_open(s.device), "opening device");

	s.chgroup = get_device_channel_group(s.device);
	enumerate_device_options("Channel group", s.driver, s.device, s.chgroup);
	s.channels = get_device_channels(s.device);

	cmd_set_samplerate(&s, 100000);
	cmd_set_sampleslimit(&s, 1024*16);

	s.session = NULL;
	assert_sr( sr_session_new(s.context, &s.session),
			"creating session");
	assert_sr( sr_session_dev_add(s.session, s.device),
			"adding device to session");

	ret = sr_session_datafeed_callback_add(s.session, on_session_datafeed, &s);
	assert_sr( ret, "adding callback for session datafeed");

	ret = sr_session_stopped_callback_set(s.session, on_session_stopped, &s);
	assert_sr( ret, "setting callback for session stopped");

	s.running = TRUE;
	assert_sr( sr_session_start(s.session), "starting session");
	g_main_loop_run(main_loop);

	assert_sr( sr_session_destroy(s.session), "destroying session");
	assert_sr( sr_dev_close(s.device), "closing device");
	assert_sr( sr_exit(s.context), "shutting down libsigrok");

	return 0;
}


