/* 
 *  Squeezelite - lightweight headless squeezebox emulator
 *
 *  (c) Adrian Smith 2012-2015, triode1@btinternet.com
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
 *
 */

// ir thread - linux only

#include "squeezelite.h"

#if IR

#include <lirc/lirc_client.h>

#define LIRC_CLIENT_ID "squeezelite"

static log_level loglevel;

struct irstate ir;

static struct lirc_config *config = NULL;
static sockfd fd = -1;

static thread_type thread;

#define LOCK_I   mutex_lock(ir.mutex)
#define UNLOCK_I mutex_unlock(ir.mutex)

// cmds based on entires in Slim_Device_Remote.ir
// these may appear as config entries in .lircrc files
struct {
	char  *cmd;
	u32_t code;
} cmdmap[] = {
	{ "voldown",  0x768900ff },
	{ "volup",    0x7689807f },
	{ "rew",      0x7689c03f },
	{ "fwd",      0x7689a05f },
	{ "pause",    0x768920df },
	{ "play",     0x768910ef },
	{ "power",    0x768940bf },
	{ "muting",   0x7689c43b },
	{ "power_on", 0x76898f70 },
	{ "power_off",0x76898778 },
	{ NULL,       0          },
};

// selected lirc namespace button names as defaults - some support repeat
struct {
	char  *lirc;
	u32_t code;
	bool  repeat;
} keymap[] = {
	{ "KEY_VOLUMEDOWN", 0x768900ff, true  },
	{ "KEY_VOLUMEUP",   0x7689807f, true  },
	{ "KEY_PREVIOUS",   0x7689c03f, false },
	{ "KEY_REWIND",     0x7689c03f, false },
	{ "KEY_NEXT",       0x7689a05f, false },
	{ "KEY_FORWARD",    0x7689a05f, false },
	{ "KEY_PAUSE",      0x768920df, true  },
	{ "KEY_PLAY",       0x768910ef, false },
	{ "KEY_POWER",      0x768940bf, false },
	{ "KEY_MUTE",       0x7689c43b, false },
	{ NULL,             0         , false },
};

static u32_t ir_cmd_map(const char *c) {
	int i;
	for (i = 0; cmdmap[i].cmd; i++) {
		if (!strcmp(c, cmdmap[i].cmd)) {
			return cmdmap[i].code;
		}
	}
	return 0;
}

static u32_t ir_key_map(const char *c, const char *r) {
	int i;
	for (i = 0; keymap[i].lirc; i++) {
		if (!strcmp(c, keymap[i].lirc)) {
			if (keymap[i].repeat || !strcmp(r, "00")) {
				return keymap[i].code;
			}
			LOG_DEBUG("repeat suppressed");
			break;
		}
	}
	return 0;
}

static void *ir_thread() {
	char *code;
	
	while (fd > 0 && lirc_nextcode(&code) == 0) {
		
		u32_t now = gettime_ms();
		u32_t ir_code = 0;
		
		if (code == NULL) continue;
		
		if (config) {
			// allow lirc_client to decode then lookup cmd in our table
			// we can only send one IR event to slimproto so break after first one
			char *c;
			while (lirc_code2char(config, code, &c) == 0 && c != NULL) {
				ir_code = ir_cmd_map(c);
				if (ir_code) {
					LOG_DEBUG("ir cmd: %s -> %x", c, ir_code);
					break;
				}
			}
		}

		if (!ir_code) {
			// try to match on lirc button name if it is from the standard namespace
			// this allows use of non slim remotes without a specific entry in .lircrc
			char *b, *r;
			strtok(code, " \n");     // discard
			r = strtok(NULL, " \n"); // repeat count
			b = strtok(NULL, " \n"); // key name
			if (r && b) {
				ir_code = ir_key_map(b, r);
				LOG_DEBUG("ir lirc: %s [%s] -> %x", b, r, ir_code);
			}
		}

		if (ir_code) {
			LOCK_I;
			if (ir.code) {
				LOG_DEBUG("code dropped");
			}
			ir.code = ir_code;
			ir.ts = now;
			UNLOCK_I;
			wake_controller();
		}
		
		free(code);
	}
	
	return 0;
}

void ir_init(log_level level, char *lircrc) {
	loglevel = level;

	fd = lirc_init(LIRC_CLIENT_ID, 0);

	if (fd > 0) {
		if (lirc_readconfig(lircrc, &config, NULL) != 0) {
			LOG_WARN("error reading config: %s", lircrc);
		}

		mutex_create(ir.mutex);

		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + IR_THREAD_STACK_SIZE);
		pthread_create(&thread, &attr, ir_thread, NULL);
		pthread_attr_destroy(&attr);

	} else {
		LOG_WARN("failed to connect to lircd - ir processing disabled");
	}
}

void ir_close(void) {
	if (fd > 0) {
		fd = -1;
		if (config) {
			lirc_freeconfig(config);
		}
		lirc_deinit();

		pthread_cancel(thread);
		pthread_join(thread, NULL);
		mutex_destroy(ir.mutex);
	}
}

#endif //#if IR
