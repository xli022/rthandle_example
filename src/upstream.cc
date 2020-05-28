
#include <log.h>
#include <upstream.h>
#include <functional>
#include <mutex>
#include <string>

namespace rthandle {

#define die_on_error(x, context)                                           \
  do {                                                                     \
    if (x < 0) {                                                           \
      LOG_ERROR("{}:{s},errno={}", context, amqp_error_string2(x), errno); \
      return -1;                                                           \
    }                                                                      \
  } while (0)

#define perror_on_error(x, context)                                          \
  do {                                                                       \
    if (x < 0) {                                                             \
      LOG_ERROR("{}: {s}, errno={}", context, amqp_error_string2(x), errno); \
    }                                                                        \
  } while (0)

#define die_on_amqp_error(x, context)                                          \
  do {                                                                         \
    switch (x.reply_type) {                                                    \
      case AMQP_RESPONSE_NORMAL:                                               \
        break;                                                                 \
                                                                               \
      case AMQP_RESPONSE_NONE:                                                 \
        LOG_ERROR("{}: missing RPC reply type!", context);                     \
        return -1;                                                             \
                                                                               \
      case AMQP_RESPONSE_LIBRARY_EXCEPTION:                                    \
        LOG_ERROR("{}: {s}", context, amqp_error_string2(x.library_error));    \
        return -1;                                                             \
                                                                               \
      case AMQP_RESPONSE_SERVER_EXCEPTION:                                     \
        switch (x.reply.id) {                                                  \
          case AMQP_CONNECTION_CLOSE_METHOD: {                                 \
            amqp_connection_close_t *m =                                       \
                (amqp_connection_close_t *)x.reply.decoded;                    \
            LOG_ERROR(                                                         \
                "{}: server connection error {}dh, message: "                  \
                "{s}",                                                         \
                context, m->reply_code, (int)m->reply_text.len,                \
                (char *)m->reply_text.bytes);                                  \
            return -1;                                                         \
          }                                                                    \
          case AMQP_CHANNEL_CLOSE_METHOD: {                                    \
            amqp_channel_close_t *m = (amqp_channel_close_t *)x.reply.decoded; \
            LOG_ERROR("{}: server channel error {}dh, message: {s}", context,  \
                      m->reply_code, (int)m->reply_text.len,                   \
                      (char *)m->reply_text.bytes);                            \
            return -1;                                                         \
          }                                                                    \
          default:                                                             \
            LOG_ERROR("{}: unknown server error, method id {8x}", context,     \
                      x.reply.id);                                             \
            return -1;                                                         \
        }                                                                      \
        return -1;                                                             \
    }                                                                          \
                                                                               \
  } while (0)

int Upstream::Open(const Upstream::Opt &opt) {
  opt_ = opt;
  return connect();
}

int Upstream::connect() {
  const std::lock_guard<std::mutex> lock(mu_);

  amqp_socket_t *socket = NULL;
  amqp_connection_state_t conn;

  if (amqp_conn_ != nullptr) {
    amqp_channel_close(amqp_conn_, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(amqp_conn_, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(amqp_conn_);
    amqp_conn_ = nullptr;
  }

  conn = amqp_new_connection();
  socket = amqp_tcp_socket_new(conn);
  if (!socket) {
    LOG_ERROR("creating TCP socket failed");
    return -1;
  }
  int status = amqp_socket_open(socket, opt_.hostname, opt_.port);
  if (status) {
    LOG_ERROR("opening TCP oscket filed");
    return -1;
  }
  amqp_conn_ = conn;
  die_on_amqp_error(
      amqp_login(conn, "/", 0, opt_.frame_max, 0, AMQP_SASL_METHOD_PLAIN,
                 opt_.username, opt_.password),
      "Logging in");

  amqp_channel_open(conn, 1);
  die_on_amqp_error(amqp_get_rpc_reply(conn), "Opening channel");

  amqp_exchange_declare(conn,                               // state
                        1,                                  // channel
                        amqp_cstring_bytes("distributor"),  // exchange
                        amqp_cstring_bytes("topic"),        // type
                        0,                                  // passive
                        1,                                  // durable
                        0,                                  // auto_delete
                        0,                                  // internal
                        amqp_empty_table                    // arguments
  );
  die_on_amqp_error(amqp_get_rpc_reply(conn), "Declaring exchange distributor");

  amqp_queue_declare(conn,                                  // state
                     1,                                     // channel
                     amqp_cstring_bytes("rthandle_queue"),  // queue
                     0,                                     // passive
                     0,                                     // durable
                     0,                                     // exclusive
                     1,                                     // auto_delete
                     amqp_empty_table                       // arguments
  );
  die_on_amqp_error(amqp_get_rpc_reply(conn), "Declaring queue rthandle_queue");

  amqp_basic_consume(conn,                                  // state
                     1,                                     // channel
                     amqp_cstring_bytes("rthandle_queue"),  // queue
                     amqp_empty_bytes,                      // consumer_tag
                     0,                                     // no_local
                     1,                                     // no_ack
                     1,                                     // exclusive
                     amqp_empty_table                       // arguments
  );
  die_on_amqp_error(amqp_get_rpc_reply(conn), "Declaring amqp_basic_consume");

  return 0;
}

int Upstream::Bind(const char *key) {
  amqp_queue_bind(amqp_conn_,                            // state
                  1,                                     // channel
                  amqp_cstring_bytes("rthandle_queue"),  // queue
                  amqp_cstring_bytes("distributor"),     // exchange
                  amqp_cstring_bytes(key),               // routing_key
                  amqp_empty_table                       // arguments
  );
  die_on_amqp_error(amqp_get_rpc_reply(amqp_conn_),
                    "Declaring queue rthandle_queue");
  return 0;
}

int Upstream::Run(
    std::function<void(const std::string &topic, const std::string &content)>
        handle) {
  int ret = 0;
  for (;;) {
    amqp_rpc_reply_t res;
    amqp_envelope_t envelope;
    amqp_connection_state_t conn = amqp_conn_;

    amqp_maybe_release_buffers(conn);
    LOG_DEBUG("Waiting message ...");
    res = amqp_consume_message(conn, &envelope, NULL, 0);
    if (AMQP_RESPONSE_NORMAL != res.reply_type) {
      ret = -1;
      break;
    }

    handle(std::string((char *)envelope.routing_key.bytes,
                       (char *)envelope.routing_key.bytes +
                           (int)envelope.routing_key.len),
           std::string((const char *)envelope.message.body.bytes,
                       (const char *)envelope.message.body.bytes +
                           envelope.message.body.len));

    amqp_destroy_envelope(&envelope);
  }
  return ret;
}

Upstream::Upstream() { amqp_conn_ = nullptr; }

Upstream::~Upstream() {
  const std::lock_guard<std::mutex> lock(mu_);
  if (amqp_conn_ != nullptr) {
    amqp_channel_close(amqp_conn_, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(amqp_conn_, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(amqp_conn_);
    amqp_conn_ = nullptr;
  }
}

}  // namespace rthandle
