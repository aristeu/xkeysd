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
#ifndef INPUT_H
#define INPUT_H
#include <stdint.h>

struct input_translate_type {
	unsigned long code;
	unsigned long type;
};
struct input_translate;

int input_translate_string(struct input_translate *priv, char *value, struct input_translate_type *type);
const char *input_translate_code(uint16_t type, uint16_t code);
struct input_translate *input_translate_init(void);
#endif	/* INPUT_H */
