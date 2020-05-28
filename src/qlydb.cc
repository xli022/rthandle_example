#include <httplib.h>
#include <log.h>
#include <mysql/mysql.h>
#include <qlydb.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctime>
#include <mutex>

namespace rthandle {

#define MAX_MYSQL_CMD_LEN 2048

MYSQL_RES *vmysql_result(MYSQL *pmysql_conn, const char *fmt, va_list ap) {
  MYSQL_RES *result = NULL;
  int res = 0;
  char cmd[MAX_MYSQL_CMD_LEN];

  if (fmt == NULL) {
    return NULL;
  }

  vsnprintf(cmd, MAX_MYSQL_CMD_LEN, fmt, ap);

  LOG_DEBUG("cmd={}", cmd);
  res = mysql_query(pmysql_conn, cmd);
  if (res != 0) {
    LOG_ERROR("query failed when {}: {}", cmd, mysql_error(pmysql_conn));
    return NULL;
  }
  result = mysql_store_result(pmysql_conn);
  if (result == NULL) {
    LOG_ERROR("store result failed when {}", cmd);
  }
  return result;
}

MYSQL_RES *mysql_result(MYSQL *pmysql_conn, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  MYSQL_RES *res = vmysql_result(pmysql_conn, fmt, ap);
  va_end(ap);
  return res;
}

int mysql_just_do(MYSQL *pmysql_conn, const char *fmt, ...) {
  int res = 0;
  char cmd[MAX_MYSQL_CMD_LEN];
  va_list argptr;

  if (fmt == NULL) {
    return -1;
  }

  va_start(argptr, fmt);
  vsnprintf(cmd, MAX_MYSQL_CMD_LEN, fmt, argptr);
  LOG_INFO("cmd={}", cmd);

  res = mysql_query(pmysql_conn, cmd);
  return res;
}

/**
 * scanf from mysql result, and return num of field it scan
 */
int mysql_nscanf_row(MYSQL_RES *result, const char *fmt_, ...) {
  char *fmt = const_cast<char *>(fmt_);
  int have_error = 0;
  va_list ap;
  int field_num = 0;
  MYSQL_ROW sql_row;
  char *ss = NULL;
  int len = 0;
  int lflag;
  char tmp[60];
  int i = 0;
  int width, base = 10;

  if (result == NULL || fmt == NULL) {
    return 0;
  }

  va_start(ap, fmt_);
  field_num = mysql_num_fields(result);
  sql_row = mysql_fetch_row(result);
  if (sql_row == NULL) {
    return 0;
  }
  for (i = 0; i < field_num; i++) {
    LOG_INFO("foud {}", sql_row[i]);
  }

  i = 0;
  while (field_num && fmt) {
    width = 0;
    lflag = 0;
    while (isspace(*fmt)) fmt++;
    if (*fmt && *fmt == '%') {
      fmt++;
      for (; *fmt; fmt++) {
        if (strchr("dibouxcsefg%%", *fmt)) break;
        if (*fmt == 'l' || *fmt == 'L')
          lflag = 1;
        else if (*fmt >= '1' && *fmt <= '9') {
          char *tc = NULL;
          for (tc = fmt; isdigit(*fmt); fmt++)
            ;
          if (fmt - tc >= 60) {
            va_end(ap);
            LOG_INFO("fmt - tc >= 60, %d", fmt - tc);
            return 0;
          }
          strncpy(tmp, tc, fmt - tc);
          tmp[fmt - tc] = '\0';
          width = atoi(tmp);
          fmt--;
        }
      }

      if (*fmt == 's') {
        ss = va_arg(ap, char *);
        len = va_arg(ap, int);
        if (width != 0 && width < len) len = width;
        if (sql_row[i]) strncpy(ss, sql_row[i], len);
        i++;
        field_num--;
      } else if (strchr("dobxu", *fmt)) {
        if (sql_row[i]) {
          if (*fmt == 'd' || *fmt == 'u')
            base = 10;
          else if (*fmt == 'x')
            base = 16;
          else if (*fmt == 'o')
            base = 8;
          else if (*fmt == 'b')
            base = 2;
          int sl = strlen(sql_row[i]);
          if (width != 0 && width > sl) {
            width = sl;
          }
          if (width > 60) {
            va_end(ap);
            LOG_INFO("with>60, {}", width);
            return 0;
          }
          LOG_INFO("strtol(tmp, NULL, {})={}", base, atoi(sql_row[i]));
          *va_arg(ap, u_int32_t *) = atoi(sql_row[i]);
        } else {
          have_error = 1;
          *va_arg(ap, uint32_t *) = 0;
        }
        i++;
        field_num--;
      } else if (*fmt == 'f') {
        if (sql_row[i]) {
          if (lflag)
            *(va_arg(ap, double *)) = atof(sql_row[i]);
          else
            *(va_arg(ap, float *)) = atof(sql_row[i]);
        } else {
          have_error = 1;
          if (lflag)
            *(va_arg(ap, double *)) = 0.0;
          else
            *(va_arg(ap, float *)) = 0.0;
        }
        i++;
        field_num--;
      }
    } else {
      va_end(ap);
      LOG_INFO("expected %%, found {} ,i=%d", *fmt, i);
      return i;
    }
    fmt++;
  }

  va_end(ap);

  return have_error == 0 ? i : 0;
}

int mysql_get_1x1(MYSQL *pmysql_conn, char *out, int len, const char *fmt_,
                  ...) {
  MYSQL_RES *result = NULL;
  va_list argptr;
  int r = 0;

  if (out == NULL || fmt_ == NULL) {
    return -1;
  }

  va_start(argptr, fmt_);
  result = vmysql_result(pmysql_conn, fmt_, argptr);
  va_end(argptr);

  if (result == NULL) {
    LOG_ERROR("result is NULL");
    return -1;
  }

  r = mysql_nscanf_row(result, "%s", out, len);
  if (r == 0) {
    LOG_WARN("row num is 0");
    mysql_free_result(result);
    return -1;
  } else if (r > 1) {
    LOG_ERROR("row num > 1: {}", r);
    mysql_free_result(result);
    return -1;
  }

  if (result) {
    mysql_free_result(result);
  }

  return 0;
}

DB::DB(const Opt &opt) {
  opt_ = opt;
  con_ = nullptr;
}

DB::~DB() {
  const std::lock_guard<std::mutex> lock(mu_);
  mysql_close(con_);
}

int DB::Open() {
  const std::lock_guard<std::mutex> lock(mu_);
  if (getcon_() == nullptr) {
    return -1;
  }
  return 0;
}

MYSQL *DB::getcon_() {
  bool init_con = false;
  unsigned int timeout_seconds = 2;

  if (con_ == nullptr) {
    con_ = mysql_init(NULL);
    init_con = true;
  }
  if (con_ == nullptr) {
    LOG_ERROR("new con failed");
    return con_;
  }

  // ok
  if (!init_con && mysql_ping(con_) == 0) {
    return con_;
  }
  mysql_options(con_, MYSQL_SET_CHARSET_NAME, "utf8");

  /* set timeout of  connection, read and write */
  mysql_options(con_, MYSQL_OPT_READ_TIMEOUT, &timeout_seconds);
  mysql_options(con_, MYSQL_OPT_WRITE_TIMEOUT, &timeout_seconds);
  timeout_seconds = 5;
  mysql_options(con_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout_seconds);

  if (mysql_real_connect(con_, opt_.host.c_str(), opt_.username.c_str(),
                         opt_.password.c_str(), "iot", opt_.port, NULL,
                         0) == NULL) {
    LOG_ERROR("mysql database cannot connect!");
    if (con_ != nullptr) {
      mysql_close(con_);
    }
    con_ = nullptr;
    return nullptr;
  }

  return con_;
}

MYSQL *DB::getcon_d_() {
  bool init_con = false;
  unsigned int timeout_seconds = 2;

  if (con_d_ == nullptr) {
    con_d_ = mysql_init(NULL);
    init_con = true;
  }
  if (con_d_ == nullptr) {
    LOG_ERROR("new con failed");
    return con_d_;
  }

  // ok
  if (!init_con && mysql_ping(con_d_) == 0) {
    return con_d_;
  }
  mysql_options(con_d_, MYSQL_SET_CHARSET_NAME, "utf8");

  /* set timeout of  connection, read and write */
  mysql_options(con_d_, MYSQL_OPT_READ_TIMEOUT, &timeout_seconds);
  mysql_options(con_d_, MYSQL_OPT_WRITE_TIMEOUT, &timeout_seconds);
  timeout_seconds = 5;
  mysql_options(con_d_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout_seconds);

  if (mysql_real_connect(con_d_, opt_.host.c_str(), opt_.username.c_str(),
                         opt_.password.c_str(), "oauth", opt_.port, NULL,
                         0) == NULL) {
    LOG_ERROR("mysql database cannot connect!");
    if (con_d_ != nullptr) {
      mysql_close(con_d_);
    }
    con_d_ = nullptr;
    return nullptr;
  }

  return con_;
}

// TODO: convert to API
int DB::GetBindUserID(const string &mac, int &uid) {
  const std::lock_guard<std::mutex> lock(mu_);

  auto con = getcon_();
  if (con == nullptr) {
    LOG_ERROR("get con failed");
    return -1;
  }

  char out[200];

  int res = mysql_get_1x1(
      con, out, sizeof(out),
      "SELECT person_id FROM device, user_bind WHERE device.`device_mac`='%s'"
      " AND device.iot_id=user_bind.`iot_id`",
      mac.c_str());
  if (res != 0) {
    LOG_ERROR("get error");
    return -1;
  }
  uid = atoi(out);

  return 0;
}

// TODO: use API
int DB::GetAmzToken(const int &uid, const string az_clientid, string &token,
                    string &refresh, string &expire) {
  MYSQL_RES *result = NULL;
  auto con = getcon_();
  if (con == nullptr) {
    LOG_ERROR("get con failed");
    return -1;
  }
  char actokenx[500] = {0};
  char refreshx[500] = {0};
  char expirex[40] = {0};
  result = mysql_result(
      con,
      "SELECT amzn_access_token, amzn_refresh_token, amzn_expires FROM "
      "oauth_uid_amzn_tokens WHERE user_id=%d AND amzn_client_id='%s'",
      uid, az_clientid.c_str());
  if (!result) {
    LOG_ERROR("mysql_result return error");
    return -1;
  }

  int r =
      mysql_nscanf_row(result, "%s%s%s", actokenx, sizeof(actokenx), refreshx,
                       sizeof(refreshx), expirex, sizeof(expirex));
  mysql_free_result(result);
  if (r != 3) {
    LOG_WARN("expected 3 result, return {}", r);
    return -1;
  }
  token = string(actokenx);
  refresh = string(refreshx);
  expire = string(expirex);

  return 0;
}

int DB::SetAmzToken(const int &uid, const string &az_clientid,
                    const string &token, const string &refresh,
                    const string &expire) {
  auto con = getcon_();
  if (con == nullptr) {
    LOG_ERROR("get con failed");
    return -1;
  }
  char actokenx[800] = {0};
  char refreshx[800] = {0};
  char clidx[800];
  char expirex[50] = {0};

  if (az_clientid.size() > 400 || token.size() > 400 || refresh.size() > 400 ||
      expire.size() > 24) {
    LOG_ERROR("too large arguments");
    return -1;
  }

  mysql_real_escape_string(con, actokenx, token.c_str(), token.size());
  mysql_real_escape_string(con, clidx, az_clientid.c_str(), az_clientid.size());
  mysql_real_escape_string(con, refreshx, refresh.c_str(), refresh.size());
  mysql_real_escape_string(con, expirex, expire.c_str(), expire.size());

  int r = mysql_just_do(
      con,
      "INSERT INTO oauth_uid_amzn_tokens(user_id, amzn_client_id, "
      "amzn_access_token, amzn_refresh_token, amzn_expires) VALUES(%d, "
      "'%s', '%s', '%s', '%s')",
      uid, clidx, actokenx, refreshx, expirex);
  if (r < 0) {
    LOG_ERROR("mysql_just_do return error");
    return -1;
  }

  return 0;
}
}  // namespace rthandle