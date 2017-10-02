#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <libsigrok/libsigrok.h>
#include <gio/gunixinputstream.h>
#include "gloscope.h"

#define UNUSED(x) (void)(x)
#define STDIN_BUFF_SIZE 80

struct state {
	char stdin_buff[STDIN_BUFF_SIZE];
	uint32_t buff_idx;
	GThread *rthread;
	gboolean running;
	struct sr_context *context;
	struct sr_dev_driver *driver;
	struct sr_dev_inst *device;
	struct sr_session *session;
	struct sr_channel **channels;
	int *positions;
	int num_channels;
	struct sr_channel_group *chgroup;
	struct gloscope_context *gloscope;
};

typedef struct state state_t;

void parse_command(state_t *, char *);
void cmd_set_samplerate(state_t *, uint64_t);
void cmd_set_sampleslimit(state_t *, uint64_t);
void cmd_set_running(state_t *, uint64_t);
void assert_sr(int, const char *);
