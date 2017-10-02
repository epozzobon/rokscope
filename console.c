#include "rokscope.h"


void cmd_set_samplerate(struct state *s, uint64_t samplerate) {
	gboolean running = s->running;
	s->running = FALSE;
	if (running) {
		assert_sr(sr_session_stop(s->session), "stopping session");
		assert_sr(sr_session_run(s->session), "running session");
	}
	GVariant *gvar;
	int ret;
	gvar = g_variant_new_uint64(samplerate);
	ret = sr_config_set(s->device, NULL, SR_CONF_SAMPLERATE, gvar);
	assert_sr( ret, "setting samplerate");
	s->running = running;
	if (running)
		assert_sr( sr_session_start(s->session) , "starting session");
}


void cmd_set_sampleslimit(struct state *s, uint64_t sampleslimit) {
	GVariant *gvar;
	int ret;
	gvar = g_variant_new_uint64(sampleslimit);
	ret = sr_config_set(s->device, NULL, SR_CONF_LIMIT_SAMPLES, gvar);
	assert_sr( ret, "setting samples limit");
}


void cmd_set_running(struct state *s, uint64_t running) {
	s->running = running > 0;
	if (running) {
		assert_sr( sr_session_start(s->session), "starting session");
	} else {
		assert_sr( sr_session_stop(s->session), "stopping session");
		assert_sr( sr_session_run(s->session), "ending session");
	}
}


char *garray_getstr(GArray *words, guint idx) {
	char *word = "";
	if (idx < words->len)
		word = g_array_index(words, char *, idx);
	if (word == NULL)
		word = "";
	return word;
}


gboolean garray_strtoull(GArray *words, guint idx, int64_t *out) {
	char *word = garray_getstr(words, idx);
	if (word[0] == 0)
		return FALSE;
	char *endptr;
	errno = 0;
	int64_t arg = strtoll(word, &endptr, 0);
	if (errno == 0) {
		out[0] = arg;
		printf("Arg is %lu\n", arg);
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
			if (garray_strtoull(words, 2, &arg)) {
				cmd_set_samplerate(s, arg);
				return TRUE;
			}
		}

		if (garray_streq("sampleslimit", words, 1)) {
			uint64_t arg;
			if (garray_strtoull(words, 2, &arg)) {
				cmd_set_sampleslimit(s, arg);
				return TRUE;
			}
		}

		if (garray_streq("running", words, 1)) {
			uint64_t arg;
			if (garray_strtoull(words, 2, &arg)) {
				cmd_set_running(s, arg);
				return TRUE;
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


