#ifndef _QL_UPSTREAM_H_
#define _QL_UPSTREAM_H_

#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <mutex>
#include <thread>

namespace rthandle {
class Upstream {
 public:
  struct Opt {
    char const *hostname;
    int port;
    char const *username;
    char const *password;
    int frame_max;
  };

  Upstream();
  virtual ~Upstream();
  Upstream(const Upstream &) = delete;
  Upstream &operator=(const Upstream &) = delete;

  // Open the upsream connection.
  // return 0 if sucess, else error.
  int Open(const Opt &opt);

  // Bind subscribe topic
  int Bind(const char *key);

  int Run(
      std::function<void(const std::string &topic, const std::string &content)>
          handle);

 private:
  int connect();

  amqp_connection_state_t amqp_conn_;
  std::mutex mu_;
  Opt opt_;
};

}  // namespace rthandle

#endif  // _QL_UPSTREAM_H_
