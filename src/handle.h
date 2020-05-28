#include <ctpl_stl.h>
#include <log.h>
#include <qlydb.h>
#include <string>
#include <vector>

namespace rthandle {

class MQHandle {
 public:
  MQHandle(int numthread, const std::string& cliid, const std::string& secret,
           DB* db);
  virtual ~MQHandle();
  virtual void Serve(const std::string& topic, const std::string& content);

 private:
  virtual void DataHandle(const std::vector<char>& topic,
                          const std::vector<char>& content);
  virtual void OnlineHandle(const std::vector<char>& topic,
                            const std::vector<char>& content);
  virtual void BindHandle(const std::vector<char>& topic,
                          const std::vector<char>& content);

  virtual int RefreshToken(const int uid, const std::string& inrefresh,
                           const std::string& clid, const std::string& secret,
                           std::string& otoken, std::string& orefresh,
                           std::string& expire);

  virtual int DpAlexaStateReport(const std::string& endpoint_id,
                                 const std::string& token, const int status);

  ctpl::thread_pool p_;

  std::string azcliid_;
  std::string azsecret_;

  DB* db_;
};

}  // namespace rthandle
