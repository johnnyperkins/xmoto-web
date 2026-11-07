/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Offline stub of the libcurl API surface xmoto uses, for the wasm build.
 * Browsers have no raw sockets, so every transfer fails with CURLE_COULDNT_CONNECT
 * and WWW.cpp takes its normal "no network" error paths. */
#ifndef XM_CURL_STUB_H
#define XM_CURL_STUB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void CURL;
typedef long long curl_off_t;

typedef enum {
  CURLE_OK = 0,
  CURLE_COULDNT_CONNECT = 7,
  CURLE_ABORTED_BY_CALLBACK = 42
} CURLcode;

typedef enum {
  CURLOPT_URL,
  CURLOPT_CONNECTTIMEOUT,
  CURLOPT_ENCODING,
  CURLOPT_FAILONERROR,
  CURLOPT_FOLLOWLOCATION,
  CURLOPT_HTTPHEADER,
  CURLOPT_HTTPPOST,
  CURLOPT_MIMEPOST,
  CURLOPT_NOPROGRESS,
  CURLOPT_NOSIGNAL,
  CURLOPT_PROGRESSDATA,
  CURLOPT_PROXY,
  CURLOPT_PROXYPORT,
  CURLOPT_PROXYTYPE,
  CURLOPT_PROXYUSERPWD,
  CURLOPT_TIMEOUT,
  CURLOPT_USERAGENT,
  CURLOPT_WRITEDATA,
  CURLOPT_WRITEFUNCTION,
  CURLOPT_XFERINFOFUNCTION
} CURLoption;

typedef enum {
  CURLPROXY_HTTP = 0,
  CURLPROXY_SOCKS4 = 4,
  CURLPROXY_SOCKS5 = 5
} curl_proxytype;

/* Select the mime code path in WWW.h; the older curl_formadd path is not stubbed. */
#define LIBCURL_VERSION_NUM 0x080000
#define CURL_VERSION_BITS(x, y, z) ((x) << 16 | (y) << 8 | (z))
#define CURL_AT_LEAST_VERSION(x, y, z) \
  (LIBCURL_VERSION_NUM >= CURL_VERSION_BITS(x, y, z))
#define CURL_ZERO_TERMINATED ((size_t)-1)

typedef struct curl_mime {
  int unused;
} curl_mime;
typedef struct curl_mimepart {
  int unused;
} curl_mimepart;
struct curl_slist {
  char *data;
  struct curl_slist *next;
};

static inline CURL *curl_easy_init(void) { return (CURL *)1; }
static inline void curl_easy_cleanup(CURL *c) { (void)c; }
#define curl_easy_setopt(handle, opt, ...) CURLE_OK
static inline CURLcode curl_easy_perform(CURL *c) {
  (void)c;
  return CURLE_COULDNT_CONNECT;
}
static inline const char *curl_easy_strerror(CURLcode c) {
  (void)c;
  return "network is not available in the browser build";
}

static curl_mime xm_stub_mime;
static curl_mimepart xm_stub_part;
static inline curl_mime *curl_mime_init(CURL *c) {
  (void)c;
  return &xm_stub_mime;
}
static inline void curl_mime_free(curl_mime *m) { (void)m; }
static inline curl_mimepart *curl_mime_addpart(curl_mime *m) {
  (void)m;
  return &xm_stub_part;
}
static inline CURLcode curl_mime_name(curl_mimepart *p, const char *n) {
  (void)p;
  (void)n;
  return CURLE_OK;
}
static inline CURLcode curl_mime_filedata(curl_mimepart *p, const char *f) {
  (void)p;
  (void)f;
  return CURLE_OK;
}
static inline CURLcode curl_mime_data(curl_mimepart *p, const char *d, size_t n) {
  (void)p;
  (void)d;
  (void)n;
  return CURLE_OK;
}

static inline struct curl_slist *curl_slist_append(struct curl_slist *l, const char *s) {
  (void)s;
  return l;
}
static inline void curl_slist_free_all(struct curl_slist *l) { (void)l; }

#ifdef __cplusplus
}
#endif

#endif
