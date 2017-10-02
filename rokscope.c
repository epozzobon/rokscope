#include "rokscope.h"


void assert_sr(int ret, const char *string) {
	if (ret != SR_OK) {
		fprintf(stderr, "Error %s (%s): %s.\n", string, sr_strerror_name(ret),
				sr_strerror(ret));
		exit(1);
	}
}


const char *configkey_tostring(uint32_t option) {
	const char *option_name;
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
		default: option_name = NULL;
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


struct sr_channel** get_device_channels(struct sr_dev_inst *dev, int *num) {
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
	num[0] = ch_count;
	return channels;
}


struct sr_dev_driver *get_driver(const char *driver_name,
		struct sr_context *sr_ctx) {
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
		if (0 == strcmp(driver->name, driver_name)) {
			int r = sr_driver_init(sr_ctx, driver);
			if (r != SR_OK) {
				fprintf(stderr, "Error initializing driver!\n");
				exit(1);
				return NULL;
			}
			
			return driver;
		}
	}
	fprintf(stderr, "%s driver not found!\n", driver_name);
	exit(1);
	return NULL;
}


struct sr_dev_inst *get_device(struct sr_dev_driver *driver) {
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


struct state reset_positions(struct state *s) {
	for (int i = 0; i < (*s).num_channels; i++) {
		(*s).positions[i] = 0;
	}
	return (*s);
}



void on_session_stopped(void *data) {
	struct state *s = data;
	s->buff_idx++;
	if (s->running)
		assert_sr( sr_session_start(s->session), "starting session");
	reset_positions(s);
}


int get_datafeed_analog_channel(const struct sr_datafeed_analog *payload) {
	GSList *channels;
	struct sr_channel *channel;

	channels = payload->meaning->channels;
	if (channels == NULL) {
		fprintf(stderr, "Channels is null?\n");
		exit(1);
	}
	if (channels->next != NULL) {
		fprintf(stderr, "Channels is > 1?\n");
		exit(1);
	}
	channel = channels->data;
	int c = channel->index;
	return c;
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

		case SR_DF_ANALOG: {
			const struct sr_datafeed_analog *payload;
			float *payload_data;

			if (s->gloscope->ready) {
				payload = packet->payload;
				payload_data = payload->data;

				int c = get_datafeed_analog_channel(payload);
				int buff_pos = s->positions[c];

				int payload_count = payload->num_samples;
				int plot_count = s->gloscope->num_samples;
				int plot_free = plot_count - buff_pos;
				int copy_count = payload_count > plot_free ? plot_free : payload_count;
				size_t copy_size = copy_count * sizeof(sample_t);

				sample_t *vert_data = s->gloscope->vert_data[c];

				memcpy(&vert_data[buff_pos], payload_data, copy_size);
				s->positions[c] = buff_pos + copy_count;

				s->gloscope->start_idx = 0;
				s->gloscope->stop_idx = plot_count-1;
			}
		} break;

		case SR_DF_LOGIC: break; // Skip this, we only want analog
		case SR_DF_END: break;

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
	
	struct gloscope_context *ctx;
	ctx = malloc(sizeof(*ctx));
	s->gloscope = ctx;
	ctx->ready = 0;
	gloscope_init(s->gloscope, s->num_channels, 4096);
	int e;
	do {
		e = gloscope_render(ctx);
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

	GInputStream *input;
	input = g_unix_input_stream_new(0, FALSE);
	g_input_stream_read_async(input, s.stdin_buff, 80, G_PRIORITY_DEFAULT,
			NULL, on_stdin_read, &s);

	s.context = NULL;
	assert_sr( sr_init(&s.context), "initializing libsigrok");

	s.driver = get_driver("hantek-6xxx", s.context);
	//s.driver = get_driver("demo", s.context);
	enumerate_device_options("Driver", s.driver, NULL, NULL);

	s.device = get_device(s.driver);
	enumerate_device_options("Device", s.driver, s.device, NULL);
	assert_sr( sr_dev_open(s.device), "opening device");

	s.chgroup = get_device_channel_group(s.device);
	enumerate_device_options("Channel group", s.driver, s.device, s.chgroup);
	s.channels = get_device_channels(s.device, &s.num_channels);
	s.positions = notnull( malloc(s.num_channels * sizeof(int)) );
	s = reset_positions(&s);

	cmd_set_samplerate(&s, 1000000);
	cmd_set_sampleslimit(&s, 4096);

	s.session = NULL;
	assert_sr( sr_session_new(s.context, &s.session), "creating session");
	assert_sr( sr_session_dev_add(s.session, s.device),
			"adding device to session");

	ret = sr_session_datafeed_callback_add(s.session, on_session_datafeed, &s);
	assert_sr( ret, "adding callback for session datafeed");

	ret = sr_session_stopped_callback_set(s.session, on_session_stopped, &s);
	assert_sr( ret, "setting callback for session stopped");

	s.rthread = g_thread_new("renderer", start_render_thread, &s);

	s.running = TRUE;
	assert_sr( sr_session_start(s.session), "starting session");
	g_main_loop_run(main_loop);

	assert_sr( sr_session_destroy(s.session), "destroying session");
	assert_sr( sr_dev_close(s.device), "closing device");
	assert_sr( sr_exit(s.context), "shutting down libsigrok");

	return 0;
}


