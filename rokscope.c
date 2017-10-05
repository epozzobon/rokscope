#include "rokscope.h"


void assert_sr(int ret, const char *string) {
	if (ret != SR_OK) {
		fprintf(stderr, "Error %s (%s): %s.\n", string, sr_strerror_name(ret),
				sr_strerror(ret));
		exit(1);
	}
}


const char *configkey_tostring(int option) {
	const char *option_name;
	switch (option) {
		case SR_CONF_LOGIC_ANALYZER: option_name = "Logic Analyzer"; break;
		case SR_CONF_OSCILLOSCOPE:   option_name = "Oscilloscope"; break;
		case SR_CONF_CONN:           option_name = "Connection"; break;
		case SR_CONF_SAMPLERATE:     option_name = "Sample Rate"; break;
		case SR_CONF_VDIV:           option_name = "Volts/Div"; break;
		case SR_CONF_COUPLING:       option_name = "Coupling"; break;
		case SR_CONF_NUM_VDIV:       option_name = "Number of Vertical Divisions"; break;
		case SR_CONF_LIMIT_MSEC:     option_name = "Sample Time Limit (ms)"; break;
		case SR_CONF_LIMIT_SAMPLES:  option_name = "Sample Number Limit"; break;
		default: option_name = NULL;
	}
	return option_name;
}


void enumerate_device_options(const char *name, struct sr_dev_driver *driver,
		struct sr_dev_inst *dev, struct sr_channel_group *chgroup) {
	GVariant *gvar;
	int res;
	GArray *options_list;

	options_list= sr_dev_options(driver, dev, chgroup);
	if (options_list == NULL) {
		fprintf(stderr, "Error getting options list from %s!\n", name);
		exit(1);
		return;
	}

	for (guint i = 0; i < options_list->len; i++) {
		uint32_t option = g_array_index(options_list, uint32_t, i);
		const char *option_name = configkey_tostring(option);

		if (option_name == NULL) {
			printf("%s option %u available", name, option);
		} else {
			printf("%s option %u available: %s", name, option, option_name);
		}

		res = sr_config_get(driver, dev, chgroup, option, &gvar);
		if (res == SR_OK) {
			gchar *value = g_variant_print(gvar, TRUE);
			printf(" = %s", value);
			free(value);
		}

		printf("\n");


		res = sr_config_list(driver, dev, chgroup, option, &gvar);
		if (res == SR_OK) {
			gchar *value = g_variant_print(gvar, TRUE);
			printf("  %s\n", value);
			free(value);
		}
	}
	g_array_free(options_list, TRUE);
}


int get_device_channel_groups(struct sr_dev_inst *dev,
		struct sr_channel_group*** chgroups_ptr) {
	struct sr_channel_group** chgroups;
	GSList *chg_list = sr_dev_inst_channel_groups_get(dev);
	if (chg_list == NULL) {
		fprintf(stderr, "Error getting channel groups list from device!\n");
		exit(1);
		return -1;
	}

	int num_groups = g_slist_length(chg_list);
	chgroups = zalloc(num_groups * sizeof(void *));
	chgroups_ptr[0] = chgroups;

	for (int i = 0; i < num_groups; i++) {
		struct sr_channel_group *group = (struct sr_channel_group *) chg_list->data;
		chgroups[i] = group;
		chg_list = chg_list->next;
	}
	return num_groups;
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


int find_rising_edge(float level, float *samples, int num_samples) {
	for (int i = 0; i < num_samples-1; i++) {
		if (samples[i] < level && samples[i+1] > level)
			return i;
	}
	return 0;
}


int find_falling_edge(float level, float *samples, int num_samples) {
	for (int i = 0; i < num_samples-1; i++) {
		if (samples[i] > level && samples[i+1] < level)
			return i;
	}
	return 0;
}


void push_buffers(struct state *s) {
	if (!s->gloscope->ready)
		return;

	int maxpos = 0;
	int skip = s->skip;
	int trigger_channel = s->trigger_channel;
	if (trigger_channel < 0 || trigger_channel >= s->num_channels)
		trigger_channel = 0;
	float *trigger_buff = s->buffers[trigger_channel] + skip;
	int trigchan_pos = s->positions[trigger_channel] - skip;

	if (s->trigger_mode == TRIGGER_RISING) {
		skip += find_rising_edge(s->trigger_level, trigger_buff, trigchan_pos);
	} else if (s->trigger_mode == TRIGGER_FALLING) {
		skip += find_falling_edge(s->trigger_level, trigger_buff, trigchan_pos);
	}

	for (int c = 0; c < s->num_channels; c++) {
		struct gloscope_plot *plot = s->gloscope->plots[c];
		size_t plot_size = (size_t) plot->num_samples;
		int end = s->positions[c];
		int count = end - skip;
		size_t size = plot_size > count ? count : plot_size;
		memcpy(plot->vert_data, s->buffers[c] + skip, size * sizeof(sample_t));
		if (maxpos < count)
			maxpos = count;
		s->positions[c] = 0;
	}

	s->gloscope->start_idx = 0;
	s->gloscope->stop_idx = maxpos - 1;

}


void on_session_stopped(void *data) {
	struct state *s = data;
	s->buff_idx++;
	if (s->running)
		assert_sr( sr_session_start(s->session), "starting session");
	push_buffers(s);
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
			sample_t *dest;

			payload = packet->payload;
			payload_data = payload->data;

			int c = get_datafeed_analog_channel(payload);
			int buff_pos = s->positions[c];

			int payload_count = payload->num_samples;
			int plot_count = (int) s->samples_limit;
			int plot_free = plot_count - buff_pos;
			int copy_count = payload_count > plot_free ? plot_free : payload_count;
			size_t copy_size = copy_count * sizeof(sample_t);

			dest = s->buffers[c];
			memcpy(&dest[buff_pos], payload_data, copy_size);
			s->positions[c] = buff_pos + copy_count;
		} break;

		case SR_DF_LOGIC: break; // Skip this, we only want analog
		case SR_DF_END: break;

		default:
			printf("unknown datafeed type %d\n", type);

	}
}


void stdin_data(struct state *s, const char *stdin_buff, size_t bytes_read) {
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
		stdin_data(s, s->stdin_buff, (size_t) bytes_read);

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
	gloscope_init(s->gloscope, s->num_channels, 1024);
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
	memset(&s, 0, sizeof(s));
	int ret;

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

	s.num_channel_groups = get_device_channel_groups(s.device, &s.chgroups);
	for (int i = 0; i < s.num_channel_groups; i++)
		enumerate_device_options("Channel group", s.driver, s.device, s.chgroups[i]);
	s.channels = get_device_channels(s.device, &s.num_channels);
	s.positions = zalloc(s.num_channels * sizeof(int));
	s.buffers = zalloc(s.num_channels * sizeof(*s.buffers));

	cmd_set_samplerate(&s, 100000);
	cmd_set_sampleslimit(&s, 4096);

	cmd_set_triggerlevel(&s, .1f);
	cmd_set_triggermode(&s, TRIGGER_RISING);
	cmd_set_voltsperdiv(&s, 0, 100, 1000);
	cmd_set_voltsperdiv(&s, 1, 100, 1000);

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


