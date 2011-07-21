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

