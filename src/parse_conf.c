#include <ctype.h>
#include <parse_conf.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

static int char2digit(char c) {
  if (isdigit(c)) {
    return c - '0';
  }
  if (c >= 'a' && c <= 'z') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'Z') {
    return c - 'A' + 10;
  }
  return 0;
}

static int put_integer2addr(void *addr, size_t addr_cap, long long int v) {
  switch (addr_cap) {
    case 1:
      *(char *)addr = (char)v;
      return 0;
    case 2:
      *(short *)addr = (short)v;
      return 0;
    case 4:
      *(int *)addr = (int)v;
      return 0;
    case 8:
      *(long long int *)addr = v;
      return 0;
    default:
      return -1;
  }
}

int conf_parse_bool(void *addr, size_t addr_cap, void *value,
                    size_t UNUSED value_len) {
  char *src = (char *)value;
  int v = 0;
  if (*src == 'y' || *src == 't' || *src == 'o') {
    v = 1;
  }
  if (put_integer2addr(addr, addr_cap, v) < 0) {
    printf("addr_cap invalid: %ld\n", addr_cap);
    return -1;
  }

  return 0;
}

int conf_parse_string(void *addr, size_t addr_cap, void *value,
                      size_t value_len) {
  char *dest = (char *)addr, *src = (char *)value;
  int len = addr_cap < value_len ? addr_cap : value_len;
  while (len-- && *src) {
    *dest++ = *src++;
  }
  *dest = '\0';

  return 0;
}

int conf_parse_integer(void *addr, size_t addr_cap, void *value,
                       size_t value_len) {
  char *src = (char *)value;
  int base = 10;
  char *pend;
  if (value_len > 2 && src[0] == '0' && src[1] == 'x') {
    base = 16;
    src += 2;
  }

  long long int valueint = strtoll(src, &pend, base);
  if (put_integer2addr(addr, addr_cap, valueint) < 0) {
    printf("invalid addr_cap: %ld\n", addr_cap);
    return -1;
  }

  return 0;
}

#define _CASE_MOVE_INT(flag1, flag2, dst, src) \
  case flag1:                                  \
  case flag2:                                  \
    dst += src;                                \
    src = 0;                                   \
    break

int conf_parse_memspace_as_bytes(void *addr, size_t addr_cap, void *value,
                                 size_t value_len) {
  size_t i;
  int base = 10;
  char *p = (char *)value;
  long long int bytes = 0, kib = 0, mib = 0, gib = 0, tib = 0, pib = 0, tmp = 0;
  for (i = 0; i < value_len && *p; i++, p++) {
    char c = *p;
    if (i == 1 && (c == 'x' || c == 'X') && tmp == 0) {
      base = 16;
      tmp = 0;
      continue;
    }

    switch (c) {
      _CASE_MOVE_INT('k', 'K', kib, tmp);
      _CASE_MOVE_INT('m', 'M', mib, tmp);
      _CASE_MOVE_INT('g', 'G', gib, tmp);
      _CASE_MOVE_INT('t', 'T', tib, tmp);
      _CASE_MOVE_INT('p', 'P', pib, tmp);
      _CASE_MOVE_INT('b', 'B', bytes, tmp);
      default:
        tmp = tmp * base + char2digit(c);
        break;
    }
  }

  bytes +=
      (kib << 10) + (mib << 20) + (gib << 30) + (tib << 40) + (pib << 50) + tmp;

  if (put_integer2addr(addr, addr_cap, bytes) < 0) {
    printf("invalid addr_cap: %ld\n", addr_cap);
    return -1;
  }
  return 0;
}

int conf_parse_time_as_second(void *addr, size_t addr_cap, void *value,
                              size_t value_len) {
  size_t i;
  int base = 10;
  char *p = (char *)value;
  long long int seconds = 0, hours = 0, minutes = 0, days = 0, years = 0,
                tmp = 0;

  for (i = 0; i < value_len && *p; i++, p++) {
    char c = *p;
    if (i == 1 && (c == 'x' || c == 'X') && tmp == 0) {
      base = 16;
      tmp = 0;
      continue;
    }

    switch (c) {
      _CASE_MOVE_INT('s', 'S', seconds, tmp);
      _CASE_MOVE_INT('m', 'M', minutes, tmp);
      _CASE_MOVE_INT('h', 'H', hours, tmp);
      _CASE_MOVE_INT('d', 'D', days, tmp);
      _CASE_MOVE_INT('y', 'Y', years, tmp);
      default:
        tmp = tmp * base + char2digit(c);
        break;
    }
  }

  seconds += years * 365 * 24 * 60 * 60 + days * 24 * 60 * 60 +
             hours * 60 * 60 + minutes * 60 + tmp;

  if (put_integer2addr(addr, addr_cap, seconds) < 0) {
    printf("invalid addr_cap: %ld\n", addr_cap);
    return -1;
  }
  return 0;
}

int conf_do_include(void *addr, size_t UNUSED addr_cap, void *value,
                    size_t UNUSED value_len) {
  if (value == NULL || strlen(value) == 0) {
    return 0;
  }
  return conf_parse_file(addr, value);
}

int conf_parse_line(parse_command_t *const cmds, const char *const line,
                    size_t line_len, const char *const confile,
                    size_t line_num) {
  static char key[CONF_MAX_LINE_LEN], value[CONF_MAX_LINE_LEN];
  char *pkey, *pvalue;
  size_t keylen = 0, valuelen = 0;
  char const *p = line;

  if (line_len == 0) {
    return 0;
  }

  // skip spaces
  while (*p && (*p == ' ' || *p == '\t')) p++;
  if (!*p) {
    printf("invalid conf at file %s, line %ld - %s\n", confile, line_num, line);
    return -1;
  }

  // command out line
  if (*p == '#' || *p == '\r' || *p == '\n' || *p == '[') {
    return 0;
  }

  // find key
  pkey = key;
  while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
    *pkey++ = *p++;
    keylen++;
  }
  *pkey = '\0';

  // skip spaces, value may empty
  while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;

  // set value
  pvalue = value;
  while (*p && *p != '\r' && *p != '\n') {
    *pvalue++ = *p++;
    valuelen++;
  }
  // trim valuestring
  while (pvalue > value && (*(pvalue - 1) == ' ' || *(pvalue - 1) == '\t')) {
    pvalue--;
  }
  *pvalue = '\0';

  // call parse_fn
  parse_command_t *it = cmds;
  for (it = cmds; it->cmd; it++) {
    if (strcmp(it->cmd, key) == 0) {
      if (it->parse_func(it->addr, it->addr_cap, value, valuelen) < 0) {
        printf("parsefunc return error, cmd %s, file %s, line %ld - %s\n",
               it->cmd, confile, line_num, line);
        return -1;
      }
    }
  }

  return 0;
}

int conf_init(parse_command_t *const cmds) {
  parse_command_t *it = cmds;
  for (it = cmds; it->cmd; it++) {
    if (it->parse_func(it->addr, it->addr_cap, (char *)it->default_value_string,
                       strlen(it->default_value_string)) < 0) {
      printf("default conf error: key=%s, value=%s\n", it->cmd,
             it->default_value_string);
      return -1;
    }
  }
  return 0;
}

int conf_parse_file(parse_command_t *const cmds, char const *const confile) {
  FILE *f = NULL;
  static char line[CONF_MAX_LINE_LEN];

  if (cmds == NULL || confile == NULL) {
    return -1;
  }

  f = fopen(confile, "r");
  if (f == NULL) {
    return -1;
  }

  int line_num = 0;
  while (fgets(line, sizeof(line), f)) {
    if (conf_parse_line(cmds, line, strlen(line), confile, line_num) < 0) {
      printf("parse conf %s failed\n", confile);
      fclose(f);
      return -1;
    }
  }

  fclose(f);
  return 0;
}

int conf_print_conf(FILE *out, parse_command_t *cmds) {
  parse_command_t *it;
  fprintf(out, "# conf parse as: \n");
  for (it = cmds; it->cmd; it++) {
    if (it->parse_func == conf_parse_string) {
      fprintf(out, "%s %s\n", it->cmd, (char *)it->addr);
    } else if (it->parse_func == conf_parse_integer ||
               it->parse_func == conf_parse_bool ||
               it->parse_func == conf_parse_memspace_as_bytes ||
               it->parse_func == conf_parse_time_as_second) {
      long long int value = 0;
      if (it->addr_cap == 1) {
        value = (long long int)*(char *)it->addr;
      } else if (it->addr_cap == 2) {
        value = (long long int)*(short *)it->addr;
      } else if (it->addr_cap == 4) {
        value = (long long int)*(int *)it->addr;
      } else if (it->addr_cap == 8) {
        value = (long long int)*(long long int *)it->addr;
      }
      fprintf(out, "%s %lld\n", it->cmd, value);
    }
  }
  return 0;
}
