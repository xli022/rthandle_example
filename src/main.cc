#include <handle.h>
#include <log.h>
#include <parse_conf_inf.h>
#include <qlydb.h>
#include <upstream.h>
#include <version.h>
#include <functional>

using namespace rthandle;

int main(int argc, char **argv) {
  // init log
  log_init();
  LOG_INFO("Starting rthandle version {}", version_str());

  if (argc != 2) {
    LOG_ERROR("USSAGE: rthandle <confile-path>");
    return -1;
  }

  Conf::Init(argv[1]);
  auto conf = Conf::Get();

  // db
  DB::Opt dbopt;
  dbopt.username = conf->config_mysql_username;
  dbopt.password = conf->config_mysql_password;
  dbopt.host = conf->config_mysql_host;
  dbopt.port = conf->config_mysql_port;
  DB db(dbopt);
  if (db.Open() < 0) {
    LOG_ERROR("open db error");
    return -1;
  }

  // init mq for real time event
  Upstream mq;
  Upstream::Opt opt;
  opt.frame_max = conf->amqp_frame_max;
  opt.hostname = conf->amqp_host;
  opt.port = conf->amqp_port;
  opt.username = conf->amqp_username;
  opt.password = conf->amqp_password;
  int r = mq.Open(opt);
  if (r < 0) {
    LOG_ERROR("error open");
    return -1;
  }
  // subscribe device data
  r = mq.Bind("log.dev.data.#");
  if (r < 0) {
    LOG_ERROR("error bind dev data");
    return -1;
  }
  // subscribe device online status
  r = mq.Bind("log.dev.status.#");
  if (r < 0) {
    LOG_ERROR("error bind dev online");
    return -1;
  }
  // device binding event, trigger when user bind/unbind to device.
  // not implement yet.
  r = mq.Bind("log.dev.bind.#");
  if (r < 0) {
    LOG_ERROR("error bind dev online");
    return -1;
  }

  // handle
  MQHandle *handle =
      new MQHandle(10, conf->amazon_client_id, conf->amazon_client_secret, &db);

  LOG_INFO("loop start...");
  mq.Run([handle](const std::string &topic, const std::string &content) {
    handle->Serve(topic, content);
  });

  LOG_INFO("end");
  delete handle;
}
