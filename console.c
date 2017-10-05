#include "rokscope.h"


gboolean save_running_state(struct state *s) {
	gboolean running = s->running;
	s->running = FALSE;
	if (running) {
		assert_sr(sr_session_stop(s->session), "stopping session");
		assert_sr(sr_session_run(s->session), "running session");
	}
	return running;
}


void restore_running_state(struct state *s, gboolean running) {
	s->running = running;
	if (running)
		assert_sr(sr_session_start(s->session) , "starting session");
}


void cmd_set_samplerate(struct state *s, uint64_t samplerate) {
	gboolean run = save_running_state(s);
	GVariant *gvar = g_variant_new_uint64(samplerate);
	int ret = sr_config_set(s->device, NULL, SR_CONF_SAMPLERATE, gvar);
	assert_sr(ret, "setting samplerate");
	s->sample_rate = samplerate;
	restore_running_state(s, run);
}


void cmd_set_sampleslimit(struct state *s, uint64_t sampleslimit) {
	GVariant *gvar = g_variant_new_uint64(sampleslimit);
	int ret = sr_config_set(s->device, NULL, SR_CONF_LIMIT_SAMPLES, gvar);
	assert_sr(ret, "setting samples limit");
	s->samples_limit = sampleslimit;

	for (int i = 0; i < s->num_channels; i++) {
		if (s->buffers[i] != NULL)
			free(s->buffers[i]);
		s->buffers[i] = notnull(malloc(sampleslimit * sizeof(sample_t)));
	}
}


void cmd_set_vdivs(struct state *s, uint64_t num_vdivs) {
	gboolean run = save_running_state(s);
	GVariant *gvar = g_variant_new_uint64(num_vdivs);
	int ret = sr_config_set(s->device, NULL, SR_CONF_NUM_VDIV, gvar);
	assert_sr(ret, "setting vdivs count");
	s->num_vdivs = num_vdivs;
	restore_running_state(s, run);
}


void cmd_set_coupling(struct state *s, uint64_t chg, const char *coupling) {
	gboolean run = save_running_state(s);
	GVariant *gvar = g_variant_new_string(coupling);
	int ret = sr_config_set(s->device, s->chgroups[chg], SR_CONF_NUM_VDIV, gvar);
	assert_sr(ret, "setting vdivs count");
	s->coupling = coupling;
	restore_running_state(s, run);
}


void cmd_set_voltsperdiv(struct state *s, uint64_t chg, uint64_t volts,
		uint64_t div) {
	gboolean run = save_running_state(s);
	GVariant *gvar_vals[2];
	GVariant *gvar;
	gvar_vals[0] = g_variant_new_uint64(volts);
	gvar_vals[1] = g_variant_new_uint64(div);
	gvar = g_variant_new_tuple(gvar_vals, 2);
	printf("%s\n", g_variant_print(gvar, TRUE));
	int ret = sr_config_set(s->device, s->chgroups[chg], SR_CONF_VDIV, gvar);
	assert_sr(ret, "setting volts/div");
	s->volts_per_div[0] = volts;
	s->volts_per_div[1] = div;
	restore_running_state(s, run);
}


void cmd_set_running(struct state *s, gboolean running) {
	s->running = running > 0;
	if (running) {
		assert_sr( sr_session_start(s->session), "starting session");
	} else {
		assert_sr( sr_session_stop(s->session), "stopping session");
		assert_sr( sr_session_run(s->session), "ending session");
	}
}


void cmd_set_skip(struct state *s, int skip) {
	s->skip = skip;
}


void cmd_set_triggermode(struct state *s, int mode) {
	s->trigger_mode = mode;
}


void cmd_set_triggerlevel(struct state *s, sample_t level) {
	s->trigger_level = level;
}


char *garray_getstr(GArray *words, guint idx) {
	char *word = "";
	if (idx < words->len)
		word = g_array_index(words, char *, idx);
	if (word == NULL)
		word = "";
	return word;
}


gboolean garray_str_to_int(GArray *words, guint idx, int64_t *out) {
	char *word = garray_getstr(words, idx);
	if (word[0] == 0)
		return FALSE;
	char *endptr;
	errno = 0;
	int64_t arg = strtoll(word, &endptr, 0);
	if (errno == 0) {
		out[0] = (uint64_t) arg;
		printf("Arg is %lu\n", arg);
		return TRUE;
	}
	return FALSE;
}


gboolean garray_str_to_uint(GArray *words, guint idx, uint64_t *out) {
	char *word = garray_getstr(words, idx);
	if (word[0] == 0)
		return FALSE;
	char *endptr;
	errno = 0;
	uint64_t arg = strtoull(word, &endptr, 0);
	if (errno == 0) {
		out[0] = (uint64_t) arg;
		printf("Arg is %lu\n", arg);
		return TRUE;
	}
	return FALSE;
}


gboolean garray_str_to_float(GArray *words, guint idx, double *out) {
	char *word = garray_getstr(words, idx);
	if (word[0] == 0)
		return FALSE;
	char *endptr;
	errno = 0;
	double arg = strtod(word, &endptr);
	if (errno == 0) {
		out[0] = arg;
		printf("Arg is %lf\n", arg);
		return TRUE;
	}
	return FALSE;
}


gboolean garray_streq(char *ref, GArray* words, guint idx) {
	char *word = garray_getstr(words, idx);
	return 0 == strcmp(word, ref);
}


gboolean execute_command(struct state *s, GArray* words) {
	if (garray_streq("set", words, 0)) {
		if (garray_streq("samplerate", words, 1)) {
			uint64_t arg;
			if (garray_str_to_uint(words, 2, &arg)) {
				cmd_set_samplerate(s, (uint64_t) arg);
				return TRUE;
			}
		}

		if (garray_streq("sampleslimit", words, 1)) {
			uint64_t arg;
			if (garray_str_to_uint(words, 2, &arg)) {
				cmd_set_sampleslimit(s, (uint64_t) arg);
				return TRUE;
			}
		}

		if (garray_streq("running", words, 1)) {
			uint64_t arg;
			if (garray_str_to_uint(words, 2, &arg)) {
				cmd_set_running(s, (gboolean) arg);
				return TRUE;
			}
		}

		if (garray_streq("triggermode", words, 1)) {
			uint64_t arg;
			if (garray_str_to_uint(words, 2, &arg)) {
				cmd_set_triggermode(s, (int) arg);
				return TRUE;
			}
		}

		if (garray_streq("triggerlevel", words, 1)) {
			double arg;
			if (garray_str_to_float(words, 2, &arg)) {
				cmd_set_triggerlevel(s, (float) arg);
				return TRUE;
			}
		}

		if (garray_streq("skip", words, 1)) {
			uint64_t arg;
			if (garray_str_to_uint(words, 2, &arg)) {
				cmd_set_skip(s, (int) arg);
				return TRUE;
			}
		}

		if (garray_streq("vdivs", words, 1)) {
			uint64_t arg;
			if (garray_str_to_uint(words, 2, &arg)) {
				cmd_set_vdivs(s, (int) arg);
				return TRUE;
			}
		}

		if (garray_streq("coupling", words, 1)) {
			uint64_t chg;
			if (garray_str_to_uint(words, 2, &chg)) {
				if (garray_streq("DC", words, 3)) {
					cmd_set_coupling(s, chg, "DC");
					return TRUE;
				}
				if (garray_streq("AC", words, 3)) {
					cmd_set_coupling(s, chg, "AC");
					return TRUE;
				}
			}
		}

		if (garray_streq("voltsperdiv", words, 1)) {
			uint64_t chg, arg3, arg4;
			if (garray_str_to_uint(words, 2, &chg)) {
				if (garray_str_to_uint(words, 3, &arg3)) {
					if (garray_str_to_uint(words, 4, &arg4)) {
						cmd_set_voltsperdiv(s, chg, arg3, arg4);
						return TRUE;
					}
				}
			}
		}
	}

	fprintf(stderr, "Command not valid\n");
	return FALSE;
}


void parse_command(struct state *s, char *cmd) {
	char *word_start = cmd;
	GArray *words = g_array_new(TRUE, TRUE, sizeof(char *));
	g_array_append_val(words, word_start);

	for (size_t i = 0; cmd[i] != 0; i++) {
		if (cmd[i] == ' ') {
			cmd[i] = 0;
			word_start = cmd + i + 1;
			g_array_append_val(words, word_start);
		}
	}

	execute_command(s, words);
	g_array_free(words, TRUE);
}


