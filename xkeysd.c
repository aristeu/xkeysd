#include <stdio.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <libconfig.h>

#include <linux/input.h>
#include <linux/uinput.h>
#include <linux/hidraw.h>

#define XKEYS_VENDOR	0x5f3
#define XKEYS_PRODUCT	0x2b1

static int find_devices(int **fds)
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
version = 1;
devices:
{
	foo:
	{
		device = "path";
		key0 = "KEY_A";
		key1 = "KEY_B";
		(...)
		key44 = "KEY_F";
		intdial = "REL_WHEEL";
		extdial = "REL_HWHEEL";
	}
	bar:
	{

	}
	(...)
}
#endif
#define XKEYS_NKEYS 46
struct device {
	int fd;
	char name[64];
	uint16_t key_mapping[XKEYS_NKEYS];
	uint16_t axle_mapping[2]; 
};

static int new_device_from_config(config_setting_t *setting)
{
	struct device *new = malloc(sizeof(struct device));
	config_setting_t *tmp;
	char keyname[6], *value;
	int i, index;

	if (new == NULL) {
		fprintf(stderr, "Error allocating memory for new device\n");
		return 1;
	}
	memset(new, 0, sizeof(*new));

	strncmp(new->name, 64, config_setting_name(setting));

	tmp = config_setting_get_member(setting, "device");
	if (tmp == NULL) {
		fprintf(stderr, "Every device must have a 'device' member\n");
		free(new);
		return 1;
	}
	snprintf(new->name, sizeof(new->name), config_setting_get_string(tmp));

	for (i = 0; i < XKEYS_NKEYS; i++) {
		sprintf(keyname, "key%i", i);
		tmp = config_setting_get_member(setting, keyname);
		if (tmp == NULL)
			continue;
		value = config_setting_get_string(tmp);
		if (value == NULL) {
			fprintf(stderr, "Error parsing key value for key%i\n", i);
			free(new);
			return 1;
		}
		new->key_mapping[i] = translate_input(value);
	}

	return 0;
}

static int read_config(void)
{
	config_t config;
	config_setting_t *root, *tmp, *devices;
	int version, i;

	//
	config_init(&config);
	ret = config_read_file(&config, filename);
	if (ret == CONFIG_FALSE) {
		fprintf(stderr, "Error reading config file %s (%s)\n",
			filename, config_error_text(&config));
		return 1;
	}

	//
	tmp = config_lookup(&config, "version");
	if (tmp == NULL) {
		fprintf(stderr, "Error config file version in %s (%s)\n",
			filename, config_error_text(&config));
		return 1;
	}
	version = config_setting_get_int(tmp);

	//
	devices = config_lookup(&config, "devices");
	if (devices == NULL) {
		fprintf(stderr, "Error getting devices block in %s (%s)\n",
			filename, config_error_text(&config));
		return 1;
	}

	//
	for (i = 0; i < HIDRAW_MAX_DEVICES; i++) {
		tmp = config_setting_get_elem(devices, i);
		if (tmp == NULL)
			break;

		if (!config_setting_is_group(tmp)) {
			fprintf(stderr, "Error in device definition for %s\n",
				config_setting_name(tmp));
			return 1;
		}
		printf("Got new device %s\n", config_setting_name(tmp));
		if (new_device_from_config(tmp))
			return 1;
	}

	config_cleanup(&config);
}

int main(int argc, char *argv[])
{

	return 0;
}

