#ifndef ADS_PARSE_CONF_LOCAL_H_
#define ADS_PARSE_CONF_LOCAL_H_

#include "parse_conf.h"

namespace rthandle {

#define CONF_HOST_LEN 118
#define CONF_URL_LEN 200
#define CONF_NAME_LEN 70
#define CONF_PASSWORD_LEN 128
#define CONF_PATH_LEN 200
#define CONF_AMZ_STRLEN 400

struct ads_main_conf_s {
  char config_mysql_host[CONF_HOST_LEN];
  int config_mysql_port;
  char config_mysql_dbname[CONF_NAME_LEN];
  char config_mysql_username[CONF_NAME_LEN];
  char config_mysql_password[CONF_PASSWORD_LEN];

  int amqp_enable;
  char amqp_host[CONF_HOST_LEN];
  int amqp_port;
  char amqp_username[CONF_NAME_LEN];
  char amqp_password[CONF_PASSWORD_LEN];
  int amqp_frame_max;

  char amazon_client_id[CONF_AMZ_STRLEN];
  char amazon_client_secret[CONF_AMZ_STRLEN];
};

typedef struct ads_main_conf_s ads_main_conf_t;

int rthandle_parse_conf(ads_main_conf_t *conf, const char *file);

class Conf : public ads_main_conf_s {
 public:
  static Conf *Get() {
    static Conf instance;
    return &instance;
  }
  static int Init(const char *confile) {
    return rthandle_parse_conf(Conf::Get(), confile);
  }

  Conf(Conf const &) = delete;
  void operator=(Conf const &) = delete;

 private:
  Conf() {}
};
}  // namespace rthandle

#endif /* ADS_PARSE_CONF_LOCAL_H_ */
