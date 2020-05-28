#ifndef PARSE_CONF_H_
#define PARSE_CONF_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CONF_MAX_LINE_LEN 1000

typedef int (*parse_fn)(void *addr, size_t addr_cap, void *value,
                        size_t value_len);

/* functions match type parse_fn */
int conf_parse_string(void *addr, size_t addr_cap, void *value,
                      size_t value_len);
int conf_parse_integer(void *addr, size_t addr_cap, void *value,
                       size_t value_len);
int conf_parse_memspace_as_bytes(void *addr, size_t addr_cap, void *value,
                                 size_t value_len);
int conf_parse_time_as_second(void *addr, size_t addr_cap, void *value,
                              size_t value_len);
int conf_do_include(void *addr, size_t addr_cap, void *value, size_t value_len);
int conf_parse_bool(void *addr, size_t addr_cap, void *value, size_t value_len);

typedef struct _parse_command_t {
  const char *cmd;
  parse_fn parse_func;
  void *addr;
  size_t addr_cap;
  const char *default_value_string;
} parse_command_t;

int conf_init(parse_command_t *const cmds);
int conf_parse_file(parse_command_t *const cmds, char const *const confile);
int conf_print_conf(FILE *out, parse_command_t *cmds);

#ifdef __cplusplus
}
#endif

#endif /* PARSE_CONF_H_ */
