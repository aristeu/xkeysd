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
