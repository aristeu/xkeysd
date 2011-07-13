#include <stdio.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <libconfig.h>

#include <linux/input.h>
#include <linux/uinput.h>
#include <linux/hidraw.h>

#include "input.h"

#define XKEYS_VENDOR	0x5f3
#define XKEYS_PRODUCT	0x2b1

static int find_devices(int *fds)
{
	int ret = 0, num, fd;
	char filename[14];
	struct hidraw_devinfo info;

	for (num = 0; num < HIDRAW_MAX_DEVICES; num++) {
		sprintf(filename, "/dev/hidraw%i", num);
		fd = open(filename, O_RDWR);
		if (fd < 0) {
			fprintf(stderr, "Error opening %s: %s\n", filename,
				strerror(errno));
			return -1;
		}
		if (ioctl(fd, HIDIOCGRAWINFO, &info) < 0) {
			perror("Error retrieving device information"); 
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

#define XKEYS_NKEYS 46
struct device {
	int fd;
	char filename[128];
	char name[64];
	uint16_t vendor;
	uint16_t product;
	uint16_t key_mapping[XKEYS_NKEYS];
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

	tmp = config_setting_get_member(setting, "device");
	if (tmp != NULL)
		snprintf(new->filename, sizeof(new->filename), config_setting_get_string(tmp));

	tmp = config_setting_get_member(setting, "vendor");
	if (tmp != NULL) {
		new->vendor = config_setting_get_integer(tmp);
		tmp = config_setting_get_member(setting, "product");
		if (tmp == NULL) {
			fprintf(stderr, "When vendor id is specified, product id must be specified too\n");
			return 1;
		}
		new->product = config_setting_get_integer(tmp);
	}

	if (strlen(new->filename) == 0 and new->vendor == 0) {
		fprintf(stderr, "Either 'device' or vendor/product ids must be supplied");
		return 1;
	}

	for (i = 0; i < XKEYS_NKEYS; i++) {
		sprintf(keyname, "key%i", i);
		tmp = config_setting_get_member(setting, keyname);
		if (tmp == NULL)
			continue;
		value = config_setting_get_string(tmp);
		if (value == NULL) {
			fprintf(stderr, "Error parsing key value for key%i\n", i);
			return 1;
		}

		if (input_translate_string(priv, value, &event)) {
			fprintf(stderr, "Unable to parse key %s\n", value);
			return 1;
		}
		if (event.type != EV_KEY) {
			fprintf(stderr, "Event %s is not supported yet, only KEY_ events\n", value);
			return 1;
		}
		new->key_mapping[i] = event.code;
	}
	tmp = config_setting_get_member(setting, "idial");
	if (tmp == NULL) {
		fprintf(stderr, "Internal dial (idial) not set\n");
	} else {
		value = config_setting_get_string(tmp);
		if (value == NULL) {
			fprintf(stderr, "Error parsing key value for idial\n");
			return 1;
		}
		if (input_translate_string(priv, value, &event)) {
			fprintf(stderr, "Unable to parse key %s\n", value);
			return 1;
		}
		if (event.type != EV_REL) {
			fprintf(stderr, "Event %s is not supported yet for idial, only REL_ events\n", value);
			return 1;
		}
		new->axle_mapping[0] = event.code;
	}

	tmp = config_setting_get_member(setting, "edial");
	if (tmp == NULL) {
		fprintf(stderr, "External dial (idial) not set\n");
	} else {
		value = config_setting_get_string(tmp);
		if (value == NULL) {
			fprintf(stderr, "Error parsing key value for edial\n");
			return 1;
		}
		if (input_translate_string(priv, value, &event)) {
			fprintf(stderr, "Unable to parse key %s\n", value);
			return 1;
		}
		if (event.type != EV_REL) {
			fprintf(stderr, "Event %s is not supported yet for edial, only REL_ events\n", value);
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
	config_setting_t *root, *tmp, *devices;
	struct input_translate *priv;
	struct device *dev;
	int version, i, ret;

	priv = input_translate_init();
	if (priv == NULL) {
		fprintf(stderr, "Error initalizing input translation library\n");
		return 1;
	}

	config_init(&config);
	ret = config_read_file(&config, filename);
	if (ret == CONFIG_FALSE) {
		fprintf(stderr, "Error reading config file %s (%s:%i)\n",
			filename, config_error_text(&config),
			config_error_line(&config));
		return 1;
	}

	tmp = config_lookup(&config, "version");
	if (tmp == NULL) {
		fprintf(stderr, "Error config file version in %s (%s)\n",
			filename, config_error_text(&config));
		return 1;
	}
	version = config_setting_get_int(tmp);

	devices = config_lookup(&config, "devices");
	if (devices == NULL) {
		fprintf(stderr, "Error getting devices block in %s (%s)\n",
			filename, config_error_text(&config));
		return 1;
	}

	for (i = 0; i < HIDRAW_MAX_DEVICES; i++) {
		tmp = config_setting_get_elem(devices, i);
		if (tmp == NULL)
			break;

		if (!config_setting_is_group(tmp)) {
			fprintf(stderr, "Error in device definition for %s\n",
				config_setting_name(tmp));
			return 1;
		}
		if (new_device_from_config(tmp, priv, &devices[device_count]))
			return 1;
		device_count++;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret, i;

	ret = read_config("sample.conf");
	if (ret != 0) {
		fprintf(stderr, "Error reading the configuration file.\n");
		return 1;
	}

	if (device_count == 0) {
		fprintf(stderr, "No devices defined on the configuration file, exiting.\n");
		return 1;
	}

	for (i = 0; i < device_count; i++) {
		ret = locate_and_open(i);
		if (ret < 0)
			fprintf(stderr, "Error opening device \"%s\"\n", devices[i].name ? devices[i].name:"noname");
		else if (ret == 0)
			/* FIXME - hotplugging */
			fprintf(stderr, "Device \"%s\" not found\n", devices[i].name ? devices[i].name:"noname");
	}

	return 0;
}

