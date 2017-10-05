#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <libsigrok/libsigrok.h>
#include <gio/gunixinputstream.h>
#include "gloscope.h"

#define UNUSED(x) (void)(x)
#define STDIN_BUFF_SIZE 80
#define TRIGGER_NONE 0
#define TRIGGER_RISING 1
#define TRIGGER_FALLING 2

struct state {
	char stdin_buff[STDIN_BUFF_SIZE];
	uint32_t buff_idx;
	GThread *rthread;
	struct sr_context *context;
	struct sr_dev_driver *driver;
	struct sr_dev_inst *device;
	struct sr_session *session;
	int *positions;
	float **buffers;
	struct sr_channel **channels;
	int num_channels;
	struct sr_channel_group **chgroups;
	int num_channel_groups;
	struct gloscope_context *gloscope;
	uint64_t samples_limit;
	uint64_t sample_rate;
	uint64_t volts_per_div[2];
	uint64_t num_vdivs;
	int trigger_mode;
	sample_t trigger_level;
	int skip;
	gboolean running;
	const char *coupling;
	int trigger_channel;
};

typedef struct state state_t;

void assert_sr(int, const char *);
void parse_command(state_t *, char *);

void cmd_set_samplerate(state_t *, uint64_t);
void cmd_set_sampleslimit(state_t *, uint64_t);
void cmd_set_running(state_t *, gboolean);
void cmd_set_voltsperdiv(state_t *, uint64_t, uint64_t, uint64_t);
void cmd_set_vdiv(state_t *, uint64_t, uint64_t, uint64_t);
void cmd_set_skip(state_t *, int);
void cmd_set_triggermode(state_t *, int);
void cmd_set_triggerlevel(state_t *, sample_t);
