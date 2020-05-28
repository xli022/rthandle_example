#ifndef QL_RTHANDLE_H_
#define QL_RTHANDLE_H_

#ifdef __cplusplus
extern "C" {
#endif

extern const char* git_version;
static inline const char* version_str() { return git_version; }

#ifdef __cplusplus
}
#endif

#endif  // QL_RTHANDLE_H_
