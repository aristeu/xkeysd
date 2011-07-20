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
	int uinput;
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
			fprintf(stderr, "When vendor id is specified, product id must be specified too\n");
			return 1;
		}
		new->product = config_setting_get_int(tmp);
	}

	if (strlen(new->filename) == 0 && new->vendor == 0) {
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
	config_setting_t *root, *tmp, *devs;
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
		fprintf(stderr, "Error reading config file: %s (%s:%i)\n",
			config_error_text(&config), filename,
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

	devs = config_lookup(&config, "devices");
	if (devs == NULL) {
		fprintf(stderr, "Error getting devices block in %s (%s)\n",
			filename, config_error_text(&config));
		return 1;
	}

	for (i = 0; i < HIDRAW_MAX_DEVICES; i++) {
		tmp = config_setting_get_elem(devs, i);
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
				fprintf(stderr, "Not enough privileges to open /dev/hidraw%i\n");
				return -1;
			}
			continue;
		}
		if (ioctl(fd, HIDIOCGRAWINFO, &info)) {
			fprintf(stderr, "Error retrieving device information from hidraw%i: %s\n",
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
		fprintf(stderr, "Error opening uinput device, exiting...\n");
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
		perror("Short write while setting up uinput device");
		goto err;
	}

	if (ioctl(dev->uinput, UI_SET_EVBIT, EV_REL)) {
		perror("Error enabling key events in uinput device");
		goto err;
	}
	for (i = 0; i < 2; i++) {
		if (ioctl(dev->uinput, UI_SET_RELBIT, dev->axle_mapping[i])) {
			fprintf(stderr, "Error enabling axis %s events: %s\n",
				input_translate_code(EV_REL, dev->axle_mapping[i]),
				strerror(errno));
			goto err;
		}
	}

	if (ioctl(dev->uinput, UI_SET_EVBIT, EV_KEY)) {
		perror("Error enabling key events in uinput device");
		goto err;
	}
	for (i = 0; i < XKEYS_NKEYS; i++) {
		if (dev->key_mapping[i] == 0)
			continue;
		if (ioctl(dev->uinput, UI_SET_KEYBIT, dev->key_mapping[i])) {
			fprintf(stderr, "Error enabling key %s in uinput device: %s\n",
				input_translate_code(EV_KEY, dev->key_mapping[i]),
				strerror(errno));
			goto err;
		}
	}

	/* in order to be seen as a mouse, we need to have REL_X, REL_Y and BTN_0 */
	if (ioctl(dev->uinput, UI_SET_RELBIT, REL_X)) {
		fprintf(stderr, "Error enabling axis X events: %s\n",
			strerror(errno));
		goto err;
	}
	if (ioctl(dev->uinput, UI_SET_RELBIT, REL_Y)) {
		fprintf(stderr, "Error enabling axis Y events: %s\n",
			strerror(errno));
		goto err;
	}
	if (ioctl(dev->uinput, UI_SET_KEYBIT, BTN_0)) {
		fprintf(stderr, "Error enabling key BTN_0 in uinput device\n",
			strerror(errno));
		goto err;
	}

	if (ioctl(dev->uinput, UI_DEV_CREATE)) {
		perror("Error creating uinput device");
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

static int write_input_event(struct device *dev, uint16_t type, uint16_t code, int32_t value)
{
	struct input_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = type;
	ev.code = code;
	ev.value = value;
	if (write(dev->uinput, &ev, sizeof(ev)) < 0) {
		perror("Error writing event to uinput device");
		return 1;
	}
	ev.type = EV_SYN;
	ev.code = SYN_REPORT;
	ev.value = 1;
	if (write(dev->uinput, &ev, sizeof(ev)) < 0) {
		perror("Error writing event to uinput device");
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
		perror("Error reading from hidraw device");
		return 1;
	}

	if (!memcmp(last, report, size))
		return 0;

	if (last[1] != 2)
		/* first run, ignore */
		goto out;

	if (report[1] != 2) {
		fprintf(stderr, "error\n");
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
			if ((lptr[byte] & bit) && (rptr[byte] & bit))
				/* key didn't change */
				continue;
			code = dev->key_mapping[i];
			value = (rptr[byte] & bit)? 1:0;
			if (write_input_event(dev, type, code, value)) {
				ret = 1;
				goto out;
			}
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
	printf("xkeysd [-c config] [-h]\n");
	printf("\t-c <config>\tuse alternate config file\n");
	printf("\t-h\t\thelp\n");
}

int main(int argc, char *argv[])
{
	int ret, i, highest, opt;
	fd_set read;
	char last[HID_MAX_DESCRIPTOR_SIZE];
	struct timeval timeout;
	const char *options = "c:h";
	char *filename = NULL;

	while ((opt = getopt(argc, argv, options)) != -1) {
		switch (opt) {
		case 'c':
			filename = strdup(optarg);
			break;
		case 'h':
			help();
			return 0;
		default:
			fprintf(stderr, "Unknown option: %c\n", opt);
			help();
			return 1;
		}
	}
	if (filename == NULL)
		filename = "/etc/xkeysd.conf";

	ret = read_config(filename);
	if (ret != 0) {
		fprintf(stderr, "Error reading the configuration file (%s)\n",
			filename);
		return 1;
	}

	if (device_count == 0) {
		fprintf(stderr, "No devices defined on the configuration file, exiting.\n");
		return 1;
	}

	for (i = 0; i < device_count; i++) {
		locate_and_open(&devices[i]);
		if (devices[i].fd < 0) {
			fprintf(stderr, "Error opening/finding device \"%s\": %s\n",
				strlen(devices[i].name) ? devices[i].name:"noname",
				strerror(errno));
			if (errno == EPERM)
				return 1;
		}
		else if (uinput_init(&devices[i])) {
			fprintf(stderr, "Error creating uinput device for device \"%s\", not using device\n",
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
			perror("Error waiting for file descriptors to become available");
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

