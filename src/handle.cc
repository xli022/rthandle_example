#include <handle.h>
#include <httplib.h>
#include <log.h>
#include <qlydb.h>
#include <time.h>
#include <functional>
#include <json.hpp>
#include <string>
#include <vector>

using std::string;
using std::vector;
using json = nlohmann::json;

namespace rthandle {

MQHandle::MQHandle(int numthread, string cliid, string secret, DB* db)
    : p_(numthread), azcliid_(cliid), azsecret_(secret), db_(db) {}
MQHandle::~MQHandle() { p_.stop(); }

void MQHandle::Serve(const std::string& topic, const std::string& content) {
  static const string data_prefix("log.dev.data.");
  static const string online_prefix("log.dev.status.");
  static const string bind_prefix("log.dev.bind.");

  if (topic.compare(0, data_prefix.size(), data_prefix) == 0) {
    p_.push([this](int, const vector<char> topic,
                   const vector<char> content) { DataHandle(topic, content); },
            vector<char>(topic.begin(), topic.end()),
            vector<char>(content.begin(), content.end()));
    return;
  }
  if (topic.compare(0, online_prefix.size(), online_prefix) == 0) {
    p_.push(
        [this](int, const vector<char> topic, const vector<char> content) {
          OnlineHandle(topic, content);
        },
        vector<char>(topic.begin(), topic.end()),
        vector<char>(content.begin(), content.end()));

    return;
  }
  if (topic.compare(0, bind_prefix.size(), bind_prefix) == 0) {
    p_.push([this](int, const vector<char> topic,
                   const vector<char> content) { BindHandle(topic, content); },
            vector<char>(topic.begin(), topic.end()),
            vector<char>(content.begin(), content.end()));
    return;
  }
}

void MQHandle::DataHandle(const vector<char>& topic,
                          const vector<char>& content) {
  // convert them back
  string topics(topic.begin(), topic.end());
  string contents(content.begin(), content.end());
  LOG_DEBUG("{} | {}", topics, contents);
  /*
    {
      "msg_type": 1,
      "message_id": "8s8s8d7f69g7f8g70",
      "product_id": 1001696948,
      "product_name": "ÒÆ¶¯¿ª¹Ø",
      "mac": "40:a5:ef:e2:a3:00",
      "dev_type": 0,
      "sub_id": "181FDC13006F0D00",
      "data": [
          {
              "key": "xxx",
              "value": "xxx",
              "tt": 123456789
          }
      ]
    }
  */

  auto msg = json::parse(contents);
  if (msg == nullptr) {
    LOG_TRACE("parse return null");
    return;
  }

  LOG_INFO("pretty: \n{}", msg.dump(4));

  // old version no msg_type
  if (msg.find("msg_type") != msg.end()) {
    LOG_TRACE("msg_type={}", msg["msg_type"]);
  }

  if (msg["mac"] != nullptr) {
    LOG_TRACE("mac={}", msg["mac"]);
  } else {
    LOG_ERROR("no mac field found");
    return;
  }

  if (msg.find("data") != msg.end()) {
    for (auto data : msg["data"]) {
      LOG_TRACE("key={:s}", data["key"]);
      LOG_TRACE("value={:s}", data["value"]);
      auto tt = data.at("tt").get<long>();
      LOG_TRACE("tt={:d}", tt);

      string dpkey(data["key"]);
      if (dpkey == "dp_alexa" || dpkey == "dp_config") {
        string value = data["value"].get<string>();
        auto dpmsg = json::parse(value);
        LOG_DEBUG("status={}", dpmsg["status"]);
        if (dpmsg["status"] == "1") {
          string mac(msg["mac"]);
          int uid;
          if (db_->GetBindUserID(mac, uid) < 0) {
            LOG_WARN("no bind user found");
            return;
          }
          LOG_DEBUG("mac {} bind {}", mac, uid);

          string token, refresh, expire;
          if (db_->GetAmzToken(uid, azcliid_, token, refresh, expire) < 0 ||
              token.size() == 0) {
            LOG_WARN("no amz token");
            return;
          }
          LOG_TRACE("token={}, refresh={}, expire={}", token, refresh, expire);
          tm brokenTime;
          strptime(expire.c_str(), "%Y-%m-%d %T", &brokenTime);
          time_t sinceEpoch = timegm(&brokenTime);
          if (sinceEpoch > time(NULL)) {
            RefreshToken(uid, refresh, azcliid_, azsecret_, token, refresh,
                         expire);
          }
          DpAlexaStateReport(mac, token, 1);
        }
      }
    }
  }
}

void MQHandle::OnlineHandle(const vector<char>& topic,
                            const vector<char>& content) {
  // convert them back
  string topics(topic.begin(), topic.end());
  string contents(content.begin(), content.end());
  LOG_DEBUG("{} | {}", topics, contents);
  // https://developer.amazon.com/en-US/docs/alexa/device-apis/alexa-endpointhealth.html
}

void MQHandle::BindHandle(const vector<char>& topic,
                          const vector<char>& content) {
  // convert them back
  string topics(topic.begin(), topic.end());
  string contents(content.begin(), content.end());
  LOG_DEBUG("{} | {}", topics, contents);
}

// https://developer.amazon.com/docs/login-with-amazon/retrieve-token-other-platforms-cbl-docs.html
int MQHandle::RefreshToken(const int uid, const string& inrefresh,
                           const string& clid, const string& secret,
                           string& otoken, string& orefresh, string& expire) {
  auto cli = httplib::Client2("https://api.amazon.com");
  const char* uri = "/auth/o2/token";
  httplib::Params params;
  httplib::Headers headers = {
      {"Content-Type", "application/x-www-form-urlencoded;charset=utf-8"}};

  params.emplace("grant_type", "refresh_token");
  params.emplace("refresh_token", inrefresh);
  params.emplace("client_id", clid);
  params.emplace("client_secret", secret);
  auto res = cli.Post(uri, headers, params);
  if (!res) {
    LOG_ERROR("post error");
    return -1;
  }
  string body(res->body);
  LOG_DEBUG("status={}, body={}", res->status, body);
  auto resjson = json::parse(body);

  string out_token = resjson["access_token"].get<string>();
  string out_type = resjson["token_type"].get<string>();
  long out_expire_in = resjson["expires_in"].get<long>();
  string out_refresh = resjson["refresh_token"].get<string>();

  long exp = out_expire_in;
  time_t exp_at = (time_t)exp + time(0);
  struct tm tmt;
  localtime_r(&exp_at, &tmt);
  char tbuf[30];
  strftime(tbuf, sizeof(tbuf), "%F %T", &tmt);
  expire = string(tbuf);
  if (out_token.size()) {
    db_->SetAmzToken(uid, clid, out_token, out_refresh, expire);
  }
  return 0;
}

// https://developer.amazon.com/en-US/docs/alexa/smarthome/send-events-to-the-alexa-event-gateway.html
// https://developer.amazon.com/en-US/docs/alexa/video/state-reporting-for-video-skills.html
// https://developer.amazon.com/en-US/docs/alexa/smarthome/state-reporting-for-a-smart-home-skill.html
int MQHandle::DpAlexaStateReport(const string& endpoint_id, const string& token,
                                 const int status) {
  const char* body = R"(
{
    "event": {
        "header": {
            "messageId": "abc-123-def-456",
            "namespace": "Alexa",
            "name": "ChangeReport",
            "payloadVersion": "3"
        },
        "endpoint": {
            "scope": {
                "type": "BearerToken",
                "token": "access-token-from-Amazon"
            },
            "endpointId": "endpoint-001",
            "cookie": {
                "path": "path/for/this/endpoint"
            }
        },
        "payload": {
            "change": {
                "cause": {
                    "type": "PHYSICAL_INTERACTION"
                },
                "properties": [
                    {
                        "namespace": "Alexa.ModeController",
                        "instance": "Washer.WashTemperature",
                        "name": "mode",
                        "value": "WashTemperature.Warm",
                        "timeOfSample": "2017-02-03T16:20:50Z",
                        "uncertaintyInMilliseconds": 0
                    }
                ]
            }
        }
    },
    "context": {
        "properties": [
            {
                "namespace": "Alexa.PowerController",
                "name": "powerState",
                "value": "ON",
                "timeOfSample": "2017-02-03T16:20:50Z",
                "uncertaintyInMilliseconds": 60000
            },
            {
                "namespace": "Alexa.EndpointHealth",
                "name": "connectivity",
                "value": {
                    "value": "OK"
                },
                "timeOfSample": "2017-02-03T16:20:50Z",
                "uncertaintyInMilliseconds": 0
            }
        ]
    }
}
)";
  auto bodyjs = json::parse(body);
  bodyjs["event"]["endpoint"]["scope"]["token"] = token;

  /*
    North America: https://api.amazonalexa.com/v3/events
    Europe: https://api.eu.amazonalexa.com/v3/events
    Far East: https://api.fe.amazonalexa.com/v3/events
  */
  const char* host = "https://api.amazonalexa.com";
  const char* uri = "/v3/events";

  auto cli = httplib::Client2(host);
  httplib::Headers headers = {{"Content-Type", "application/json"},
                              {"Authorization", "Bearer " + token}};

  auto res = cli.Post(uri, headers, string(bodyjs.dump()), "application/json");
  if (!res) {
    LOG_ERROR("post error");
    return -1;
  }
  string resbody(res->body);
  LOG_DEBUG("status={}, body={}", res->status, resbody);

  return 0;
}

}  // namespace rthandle
