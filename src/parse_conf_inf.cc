#include <parse_conf.h>
#include <parse_conf_inf.h>

namespace rthandle {

#define CONF_CMD_END() \
  { NULL, NULL, NULL, 0, NULL }
#define CONF_CMD_BEGIN() \
  { "include", conf_do_include, commands, 0, "" }
#define CONF_CMD(conf, key, func, default_value) \
  { #key, func, &conf->key, sizeof(conf->key), default_value }
#define CONF_CMD_INT(conf, key, default_value) \
  CONF_CMD(conf, key, conf_parse_integer, default_value)
#define CONF_CMD_STR(conf, key, default_value) \
  CONF_CMD(conf, key, conf_parse_string, default_value)
#define CONF_CMD_MEM(conf, key, default_value) \
  CONF_CMD(conf, key, conf_parse_memspace_as_bytes, default_value)
#define CONF_CMD_TIME(conf, key, default_value) \
  CONF_CMD(conf, key, conf_parse_time_as_second, default_value)
#define CONF_CMD_BOOL(conf, key, default_value) \
  CONF_CMD(conf, key, conf_parse_bool, default_value)

int rthandle_parse_conf(ads_main_conf_t *conf, const char *file) {
  parse_command_t commands[] = {
      CONF_CMD_BEGIN(),

      CONF_CMD_STR(conf, config_mysql_host, "127.0.0.1"),
      CONF_CMD_INT(conf, config_mysql_port, "3306"),
      CONF_CMD_STR(conf, config_mysql_dbname, "iot"),
      CONF_CMD_STR(conf, config_mysql_username, "user"),
      CONF_CMD_STR(conf, config_mysql_password, "password"),

      CONF_CMD_BOOL(conf, amqp_enable, "yes"),
      CONF_CMD_STR(conf, amqp_host, "127.0.0.1"),
      CONF_CMD_INT(conf, amqp_port, "5672"),
      CONF_CMD_STR(conf, amqp_username, "guest"),
      CONF_CMD_STR(conf, amqp_password, "guest"),
      CONF_CMD_MEM(conf, amqp_frame_max, "10m"),

      CONF_CMD_STR(conf, amazon_client_id, "default-client-id"),
      CONF_CMD_STR(conf, amazon_client_secret, "default-secret"),

      CONF_CMD_END(),
  };

  if (conf_init(commands) < 0) {
    return -1;
  }
  if (conf_parse_file(commands, file) < 0) {
    return -1;
  }
  conf_print_conf(stdout, commands);
  return 0;
}

}  // namespace rthandle
