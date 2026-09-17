/* ocp_parse.h — bounded whole-token parsers for hostile command fields. */
#ifndef OSCILLA_OCP_PARSE_H
#define OSCILLA_OCP_PARSE_H

#include <stdbool.h>
#include <stdint.h>

bool ocp_parse_u32(const char *text, uint32_t *out);

#endif
