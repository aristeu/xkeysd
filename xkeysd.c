/*
 *    This file is part of xkeysd.
 *
 *    xkeysd is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    xkeysd is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with xkeysd; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <stdio.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <glob.h>
#include <syslog.h>

#include <libconfig.h>

#include <linux/input.h>
#include <linux/uinput.h>
#include <linux/hidraw.h>

#include "input.h"

#define XKEYS_VENDOR	0x5f3
#define XKEYS_PRODUCT	0x2b1

#ifndef UINPUT_FILE
#error Please define UINPUT_FILE in Makefile
#endif

#define BITS_PER_LONG (sizeof(unsigned long) * 8)
static unsigned long open_hidraw_devices[(HIDRAW_MAX_DEVICES/BITS_PER_LONG) + 1];
static void set_ohd_bit(int num)
{
	int i = (num / BITS_PER_LONG);

	open_hidraw_devices[i] |= (1 << num % BITS_PER_LONG);
}

static int get_ohd_bit(int num)
{
	int i = (num / BITS_PER_LONG);

	return (open_hidraw_devices[i] & (1 << num % BITS_PER_LONG));
}

int run_as_daemon;

static int log_init(void)
{
	if (run_as_daemon)
		openlog("xkeysd", LOG_CONS, LOG_DAEMON);
}

#define log(x...) do { \
	if (run_as_daemon) \
		syslog(LOG_DAEMON|LOG_NOTICE, #x); \
	else \
		printf(#x); \
	} while(0)

#define log_err(x...) do { \
	if (run_as_daemon) \
		syslog(LOG_DAEMON|LOG_ERR, #x); \
	else \
		fprintf(stderr, #x); \
	} while(0)

static int find_devices(int *fds)
{
	int ret = 0, num, fd;
	char filename[14];
	struct hidraw_devinfo info;

	for (num = 0; num < HIDRAW_MAX_DEVICES; num++) {
		sprintf(filename, "/dev/hidraw%i", num);
		fd = open(filename, O_RDWR);
		if (fd < 0) {
			log_err("Error opening %s: %s\n", filename,
				strerror(errno));
			return -1;
		}
		if (ioctl(fd, HIDIOCGRAWINFO, &info) < 0) {
			log_err("Error retrieving device information (%s)\n", strerror(errno)); 
			return -1;
		}
		if (info.vendor != XKEYS_VENDOR || info.product != XKEYS_PRODUCT) {
			close(fd);
			continue;
		}

		fds[ret++] = fd;
	}
	return ret;
}

static int grab_devices(int *ev_fds, int max)
{
	glob_t buf; 
	struct input_id id;
	int i, fd, found = 0;

	if (glob("/dev/input/event*", 0, NULL, &buf))
		return -1;

	for (i = 0; i < buf.gl_pathc; i++) {
		fd = open(buf.gl_pathv[i], O_RDWR);
		if (fd < 0) {
			log_err("Unable to open event device (%s)\n", strerror(errno));
			return -1;
		}

		if (ioctl(fd, EVIOCGID, &id)) {
			log_err("Unable to fetch information about event device (%s)\n", strerror(errno));
			return -1;
		}

		if (id.vendor != XKEYS_VENDOR || id.product != XKEYS_PRODUCT || id.bustype == BUS_VIRTUAL) {
			close(fd);
			continue;
		}

		if (ioctl(fd, EVIOCGRAB, 1))
			log_err("Unable to grab event device (%s)\n", strerror(errno));
		ev_fds[found++] = fd;

		if (found >= max) {
			log_err("Max devices reached, ignoring others\n");
			return found;
		}
	}
	globfree(&buf);

	return found;
}

#if 0
device definition
{
	method = [evdev|hidraw];		/* default evdev */

	vendor = <id>;			\
	product = <id>;			|
	bus = <id>;			|---
	serial = <id>;			/   \
					     +----- either
	device = <file>;		>---/

	keyX = ...;
	rel_axisX = ...;
	abs_axisY = ...;
}
add to struct device:
	- serial (for device serial name)
	- method (evdev, hidraw)
#endif

#define MAX_PRESSED_KEYS	10
struct key_map;
struct key_map {
	uint16_t code[MAX_PRESSED_KEYS];
	struct key_map *next;
};

#define XKEYS_NKEYS 46
struct device {
	int fd;
	int uinput;
	char filename[128];
	char name[64];
	uint16_t vendor;
	uint16_t product;
	struct key_map key_mapping[XKEYS_NKEYS];
	uint16_t axle_mapping[2]; 
	uint16_t last_axle_value[2];
};

static int new_device_from_config(config_setting_t *setting, struct input_translate *priv, struct device *new)
{
	struct input_translate_type event;
	config_setting_t *tmp;
	char keyname[6], *value;
	int i, index;

	new->fd = -1;

	tmp = config_setting_get_member(setting, "name");
	if (tmp != NULL)
		snprintf(new->name, sizeof(new->name), config_setting_get_string(tmp));
	else
		strncpy(new->name, "noname", sizeof(new->name));

	tmp = config_setting_get_member(setting, "device");
	if (tmp != NULL)
		snprintf(new->filename, sizeof(new->filename), config_setting_get_string(tmp));

	tmp = config_setting_get_member(setting, "vendor");
	if (tmp != NULL) {
		new->vendor = config_setting_get_int(tmp);
		tmp = config_setting_get_member(setting, "product");
		if (tmp == NULL) {
			log_err("When vendor id is specified, product id must be specified too\n");
			return 1;
		}
		new->product = config_setting_get_int(tmp);
	}

	if (strlen(new->filename) == 0 && new->vendor == 0) {
		log_err("Either 'device' or vendor/product ids must be supplied");
		return 1;
	}

	for (i = 0; i < XKEYS_NKEYS; i++) {
		const char *delim1 = ";", *delim2 = "+";
		char *tmp1, *tmp2, *saved1, *saved2, *token;
		int j;
		struct key_map *prev = NULL, *cur;

		sprintf(keyname, "key%i", i);
		tmp = config_setting_get_member(setting, keyname);
		if (tmp == NULL)
			continue;
		value = config_setting_get_string(tmp);
		if (value == NULL) {
			log_err("Error parsing key value for key%i\n", i);
			return 1;
		}

		/*
		 * key config format works like this:
		 * key12 = KEY_LEFTALT+KEY_T;KEY_LEFTCTRL+KEY_LEFTALT+KEY_DELETE;KEY_A
		 * will generate alt+t, ctrl+alt+del, a
		 * everything in one block (delimited by ';') will be pressed then
		 * released at the same time. this means that each ";" represents a
		 * "release all keys". This means that:
		 *	key12 = KEY_LEFTCTRL;KEY_LEFTALT;KEY_DELETE
		 * is different from
		 * 	key12 = KEY_LEFTCTRL+KEY_LEFTALT+KEY_DELETE
		 * in the sense that the former will press and release each of the
		 * keys separately while the last will press all of them then release
		 * all of them.
		 *
		 * The limit of keys pressed is controlled by MAX_KEYS_PRESSED
		 */
		for (tmp1 = value; ; tmp1 = NULL) {
			token = strtok_r(tmp1, delim1, &saved1);
			if (token == NULL)
				break;

			if (prev == NULL)
				cur = &new->key_mapping[i];
			else {
				cur = malloc(sizeof(*cur));
				if (cur == NULL) {
					log_err("Not enought memory\n");
					exit(1);
				}
				memset(cur, 0, sizeof(*cur));
				prev->next = cur;
			}
			for (j = 0, tmp2 = token; ; tmp2 = NULL, j++) {
				token = strtok_r(tmp2, delim2, &saved2);
				if (token == NULL)
					break;

				if (input_translate_string(priv, token, &event)) {
					log_err("Unable to parse key %s\n", token);
					return 1;
				}
				if (event.type != EV_KEY) {
					log_err("Event %s is not supported yet, only KEY_ events\n", token);
					return 1;
				}
				if (j >= MAX_PRESSED_KEYS) {
					log_err("Maximum of pressed keys reached (%i)\n", MAX_PRESSED_KEYS);
					return 1;
				}
				cur->code[j] = event.code;
			}
			prev = cur; 
		}
	}
	tmp = config_setting_get_member(setting, "idial");
	if (tmp == NULL) {
		log_err("Internal dial (idial) not set\n");
	} else {
		value = config_setting_get_string(tmp);
		if (value == NULL) {
			log_err("Error parsing key value for idial\n");
			return 1;
		}
		if (input_translate_string(priv, value, &event)) {
			log_err("Unable to parse key %s\n", value);
			return 1;
		}
		if (event.type != EV_REL) {
			log_err("Event %s is not supported yet for idial, only REL_ events\n", value);
			return 1;
		}
		new->axle_mapping[0] = event.code;
	}

	tmp = config_setting_get_member(setting, "edial");
	if (tmp == NULL) {
		log_err("External dial (idial) not set\n");
	} else {
		value = config_setting_get_string(tmp);
		if (value == NULL) {
			log_err("Error parsing key value for edial\n");
			return 1;
		}
		if (input_translate_string(priv, value, &event)) {
			log_err("Unable to parse key %s\n", value);
			return 1;
		}
		if (event.type != EV_REL) {
			log_err("Event %s is not supported yet for edial, only REL_ events\n", value);
			return 1;
		}
		new->axle_mapping[1] = event.code;
	}

	return 0;
}

static struct device devices[HIDRAW_MAX_DEVICES];
static int device_count;

static int read_config(char *filename)
{
	config_t config;
	config_setting_t *root, *tmp, *devs;
	struct input_translate *priv;
	struct device *dev;
	int version, i, ret;

	priv = input_translate_init();
	if (priv == NULL) {
		log_err("Error initalizing input translation library\n");
		return 1;
	}

	config_init(&config);
	ret = config_read_file(&config, filename);
	if (ret == CONFIG_FALSE) {
		log_err("Error reading config file: %s (%s:%i)\n",
			config_error_text(&config), filename,
			config_error_line(&config));
		return 1;
	}

	tmp = config_lookup(&config, "version");
	if (tmp == NULL) {
		log_err("Error config file version in %s (%s)\n",
			filename, config_error_text(&config));
		return 1;
	}
	version = config_setting_get_int(tmp);

	devs = config_lookup(&config, "devices");
	if (devs == NULL) {
		log_err("Error getting devices block in %s (%s)\n",
			filename, config_error_text(&config));
		return 1;
	}

	for (i = 0; i < HIDRAW_MAX_DEVICES; i++) {
		tmp = config_setting_get_elem(devs, i);
		if (tmp == NULL)
			break;

		if (!config_setting_is_group(tmp)) {
			log_err("Error in device definition for %s\n",
				config_setting_name(tmp));
			return 1;
		}
		if (new_device_from_config(tmp, priv, &devices[device_count]))
			return 1;
		device_count++;
	}

	return 0;
}


static int hidraw_search(int vendor, int product)
{
	int fd = -1, i;
	char filename[] = "/dev/hidrawXXX";
	struct hidraw_devinfo info;

	for (i = 0; i < HIDRAW_MAX_DEVICES; i++) {
		if (get_ohd_bit(i))
			continue;
		snprintf(filename, sizeof(filename) + 1, "/dev/hidraw%i",
			 i);
		fd = open(filename, O_RDWR);
		if (fd < 0) {
			if (errno == EPERM) {
				log_err("Not enough privileges to open /dev/hidraw%i\n");
				return -1;
			}
			continue;
		}
		if (ioctl(fd, HIDIOCGRAWINFO, &info)) {
			log_err("Error retrieving device information from hidraw%i: %s\n",
				i, strerror(errno));
			close(fd);
			continue;
		}
		if (info.vendor == vendor && info.product == product) {
			set_ohd_bit(i);
			return fd;
		}
	}
	close(fd);
	return -1;
}

static void locate_and_open(struct device *dev)
{
	if (strlen(dev->filename))
		dev->fd = open(dev->filename, O_RDONLY);
	else
		dev->fd = hidraw_search(dev->vendor, dev->product);
}

/* TODO: get rid of dev->name kludge */
static int uinput_init(struct device *dev)
{
	struct uinput_user_dev udev;
	int i;

	dev->uinput = open(UINPUT_FILE, O_RDWR);
	if (dev->uinput < 0) {
		log_err("Error opening uinput device, exiting...\n");
		return -1;
	}

	memset(&udev, 0, sizeof(udev));

	snprintf(udev.name, sizeof(udev.name), "xkeysd device (%s)",
		strlen(dev->name) ? dev->name:"noname");

	udev.id.bustype = BUS_VIRTUAL;
	udev.id.vendor = XKEYS_VENDOR;
	udev.id.product = XKEYS_PRODUCT;
	udev.id.version = 1;

	if (write(dev->uinput, &udev, sizeof(udev)) != sizeof(udev)) {
		log_err("Short write while setting up uinput device (%s)\n", strerror(errno));
		goto err;
	}

	if (ioctl(dev->uinput, UI_SET_EVBIT, EV_REL)) {
		log_err("Error enabling key events in uinput device (%s)\n", strerror(errno));
		goto err;
	}
	for (i = 0; i < 2; i++) {
		if (ioctl(dev->uinput, UI_SET_RELBIT, dev->axle_mapping[i])) {
			log_err("Error enabling axis %s events: %s\n",
				input_translate_code(EV_REL, dev->axle_mapping[i]),
				strerror(errno));
			goto err;
		}
	}

	if (ioctl(dev->uinput, UI_SET_EVBIT, EV_KEY)) {
		log_err("Error enabling key events in uinput device (%s)\n", strerror(errno));
		goto err;
	}
	for (i = 0; i < XKEYS_NKEYS; i++) {
		int j;
		struct key_map *cur;

		for (cur = &dev->key_mapping[i]; cur; cur = cur->next) {
			for (j = 0; j < MAX_PRESSED_KEYS; j++) {
				if (cur->code[j] == 0)
					continue;
				if (ioctl(dev->uinput, UI_SET_KEYBIT, cur->code[j])) {
					log_err("Error enabling key %s in uinput device: %s\n",
						input_translate_code(EV_KEY, cur->code[j]),
						strerror(errno));
					goto err;
				}
			}
		}
	}

	/* in order to be seen as a mouse, we need to have REL_X, REL_Y and BTN_0 */
	if (ioctl(dev->uinput, UI_SET_RELBIT, REL_X)) {
		log_err("Error enabling axis X events: %s\n",
			strerror(errno));
		goto err;
	}
	if (ioctl(dev->uinput, UI_SET_RELBIT, REL_Y)) {
		log_err("Error enabling axis Y events: %s\n",
			strerror(errno));
		goto err;
	}
	if (ioctl(dev->uinput, UI_SET_KEYBIT, BTN_0)) {
		log_err("Error enabling key BTN_0 in uinput device\n",
			strerror(errno));
		goto err;
	}

	if (ioctl(dev->uinput, UI_DEV_CREATE)) {
		log_err("Error creating uinput device (%s)\n", strerror(errno));
		goto err;
	}

	return 0;
err:
	close(dev->uinput);
	dev->uinput = -1;
	return -1;
}

static int select_init(fd_set *set, struct device *devices, int count)
{
	int i, highest = -1;

	FD_ZERO(set);
	for (i = 0; i < count; i++)
		if (devices[i].fd >= 0) {
			if (devices[i].fd > highest)
				highest = devices[i].fd;
			FD_SET(devices[i].fd, set);
		}
	return highest;
}

static int _write_input_event(struct device *dev, uint16_t type, uint16_t code, int32_t value)
{
	struct input_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = type;
	ev.code = code;
	ev.value = value;
	if (write(dev->uinput, &ev, sizeof(ev)) < 0) {
		log_err("Error writing event to uinput device (%s)\n", strerror(errno));
		return 1;
	}
	return 0;
}

static int write_input_event(struct device *dev, uint16_t type, uint16_t code, int32_t value)
{
	if (_write_input_event(dev, type, code, value))
		return 1;

	return _write_input_event(dev, EV_SYN, SYN_REPORT, 1);
}

static int run_macro_map(struct key_map *cur, int value, struct device *dev, uint16_t type)
{
	int j, code;

	for (j = 0; j < MAX_PRESSED_KEYS; j++) {
		code = cur->code[j];
		if (code == 0)
			break;
		if (_write_input_event(dev, type, code, value)) {
			return 1;
		}
	}
	return _write_input_event(dev, EV_SYN, SYN_REPORT, 1);
}

static int run_macro(struct key_map *map, int val, struct device *dev, uint16_t type)
{
	struct key_map *cur;
	int j, code, value = val? 1:0, multiple = 0;

	for (cur = map; cur; cur = cur->next) {
		if (cur->next != NULL || multiple) {
			/* if there're multiple blocks, we don't support
			 * press/release */
			multiple = 1;

			/*
			 * since we already released the keys previously,
			 * there's no need to generate key release events
			 */
			if (value == 0)
				break;
		}

		if (run_macro_map(cur, value, dev, type))
			return 1;

		if (multiple)
			/* multiple blocks, issue key release right now */
			if (run_macro_map(cur, 0, dev, type))
				return 1;
	}
	return 0;
}

#define SHUTTLE	2
#define JOG	3
#define KEYS	4

/*
 *  0   7  14  18  22  26  30  37  44
 *  1   8  15  19  23  27  31  38  45
 *
 *  2   9   16  20  24  28  32  39
 *  3  10   17  21  25  29  33  40
 *  4  11                   34  41
 *  5  12                   35  42
 *  6  13                   36  43
 */
static struct {
	unsigned char byte;
	unsigned char bit;
} xkeys_key_bits[] = {
	{ 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 4 }, { 0, 5 }, { 0, 6 },
	{ 1, 0 }, { 1, 1 }, { 1, 2 }, { 1, 3 }, { 1, 4 }, { 1, 5 }, { 1, 6 },
	{ 2, 0 }, { 2, 1 }, { 2, 2 }, { 2, 3 },
	{ 3, 0 }, { 3, 1 }, { 3, 2 }, { 3, 3 },
	{ 4, 0 }, { 4, 1 }, { 4, 2 }, { 4, 3 },
	{ 5, 0 }, { 5, 1 }, { 5, 2 }, { 5, 3 },
	{ 6, 0 }, { 6, 1 }, { 6, 2 }, { 6, 3 }, { 6, 4 }, { 6, 5 }, { 6, 6 },
	{ 7, 0 }, { 7, 1 }, { 7, 2 }, { 7, 3 }, { 7, 4 }, { 7, 5 }, { 7, 6 },
	{ 8, 0 }, { 8, 1 },
};
static int device_input(struct device *dev, char *last)
{
	int ret = 0, i, size;
	uint16_t type, code;
	int32_t value;
	char report[HID_MAX_DESCRIPTOR_SIZE], *rptr, *lptr;

	size = read(dev->fd, report, sizeof(report));
	if (size < 0) {
		log_err("Error reading from hidraw device (%s)\n", strerror(errno));
		return 1;
	}

	if (!memcmp(last, report, size))
		return 0;

	if (last[1] != 2)
		/* first run, ignore */
		goto out;

	if (report[1] != 2) {
		log_err("error\n");
		exit(1);
	}
	if (report[SHUTTLE] != last[SHUTTLE]) {
		type = EV_REL;
		code = dev->axle_mapping[1];
		value = report[SHUTTLE] - last[SHUTTLE];
		goto send;
	}
	if (report[JOG] != last[JOG]) {
		type = EV_REL;
		code = dev->axle_mapping[0];
		value = report[JOG] - last[JOG];
		goto send;
	}
	if (memcmp(&report[KEYS], &last[KEYS], 9)) {
		unsigned char byte, bit;
		type = EV_KEY;
		rptr = &report[KEYS];
		lptr = &last[KEYS];
		for (i = 0; i < XKEYS_NKEYS; i++) {
			byte = xkeys_key_bits[i].byte;
			bit = 1 << xkeys_key_bits[i].bit;
			if ((lptr[byte] & bit) == (rptr[byte] & bit))
				/* key didn't change */
				continue;
			ret = run_macro(&dev->key_mapping[i],
					(rptr[byte] & bit), dev, type);
			if (ret)
				goto out;
		}
	}

out:
	memcpy(last, report, size);
	return ret;
send:
	ret = write_input_event(dev, type, code, value);
	goto out;
}

static void help(void)
{
	printf("xkeysd [-c config] [-d] [-h]\n");
	printf("\t-c <config>\tuse alternate config file\n");
	printf("\t-d\t\tbecome a daemon and detach from the controlling terminal\n");
	printf("\t-h\t\thelp\n");
}

/* max number of event devices grabbed */
#define EV_FDS_SIZE 10
int main(int argc, char *argv[])
{
	int ret, i, highest, opt, d = 0;
	fd_set read;
	char last[HID_MAX_DESCRIPTOR_SIZE];
	int ev_fds[EV_FDS_SIZE];
	struct timeval timeout;
	const char *options = "c:dh";
	char *filename = NULL;

	while ((opt = getopt(argc, argv, options)) != -1) {
		switch (opt) {
		case 'c':
			filename = strdup(optarg);
			break;
		case 'd':
			d = 1;
			break;
		case 'h':
			help();
			return 0;
		default:
			log_err("Unknown option: %c\n", opt);
			help();
			return 1;
		}
	}
	if (filename == NULL)
		filename = "/etc/xkeysd.conf";

	if (d) {
		if (daemon(0, 0) == -1) {
			log_err("Error forking process (%s)\n", strerror(errno));
			return 1;
		}
	}

	ret = read_config(filename);
	if (ret != 0) {
		log_err("Error reading the configuration file (%s)\n",
			filename);
		return 1;
	}

	if (device_count == 0) {
		log_err("No devices defined on the configuration file, exiting.\n");
		return 1;
	}

	if (grab_devices(ev_fds, EV_FDS_SIZE) < 0) {
		log_err("Unable to grab devices, exiting\n");
		return 1;
	}

	for (i = 0; i < device_count; i++) {
		locate_and_open(&devices[i]);
		if (devices[i].fd < 0) {
			log_err("Error opening/finding device \"%s\": %s\n",
				strlen(devices[i].name) ? devices[i].name:"noname",
				strerror(errno));
			if (errno == EPERM)
				return 1;
		}
		else if (uinput_init(&devices[i])) {
			log_err("Error creating uinput device for device \"%s\", not using device\n",
				strlen(devices[i].name) ? devices[i].name:"noname",
				strerror(errno));
			close(devices[i].fd);
			devices[i].fd = -1;
		}
	}

	memset(last, 0, sizeof(last));
	while(1) {
		highest = select_init(&read, devices, device_count);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		ret = select(highest + 1, &read, NULL, NULL, &timeout);	
		if (ret == 0)
			continue;
		if (ret < 0) {
			log_err("Error waiting for file descriptors to become available (%s)\n", strerror(errno));
			return 1;
		}
		for (i = 0; i < device_count; i++) {
			if (FD_ISSET(devices[i].fd, &read))
				if (device_input(&devices[i], last))
					return 1;
		}
	}

	return 0;
}

