#include <stdio.h>
#include <linux/input.h>

#include "input.h"

int main(int argc, char *argv[])
{
	struct input_translate *data;
	struct input_translate_type type;


	data = input_translate_init();
	if (input_translate_string(data, "KEY_POWER", &type))
		return 1;

	printf("KEY_POWER is %i/%i\n", type.type, type.code);
	printf("and string is %s (%i/%i)\n", input_translate_code(EV_KEY, KEY_POWER), EV_KEY, KEY_POWER);

	return 0;
}

