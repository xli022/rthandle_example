#ifndef _QLYDB_H_
#define _QLYDB_H_

#include <log.h>
#include <mysql/mysql.h>
#include <mutex>
#include <string>
#include <vector>

using std::string;
using std::vector;

namespace rthandle {

class DB {
 public:
  struct Opt {
    string username;
    string password;
    string host;
    string apibaseurl;
    int port;
  };

  DB(const Opt& opt);
  int Open();
  ~DB();

  DB(DB const&) = delete;
  void operator=(DB const&) = delete;

  int GetBindUserID(const string& mac, int& uid);

  int GetAmzToken(const int& uid, const string az_clientid, string& token,
                  string& refresh, string& expire);
  int SetAmzToken(const int& uid, const string& az_clientid,
                  const string& token, const string& refresh,
                  const string& expire);

 private:
  MYSQL* getcon_();
  MYSQL* getcon_d_();

  std::mutex mu_;
  Opt opt_;
  MYSQL* con_;
  MYSQL* con_d_;
};

}  // namespace rthandle

#endif  // _QLYDB_H_
