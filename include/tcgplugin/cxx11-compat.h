#ifndef _CXX_COMPAT_H
#define _CXX_COMPAT_H

//C++11 compatibility stuff
#define container_of(ptr, type, member)   \
    ((type *) ((char *) (ptr) - offsetof(type, member)))

#define register

#if __STDC_VERSION__ >= 201112L || __cplusplus >= 201103L
typedef struct {
  long long __clang_max_align_nonce1
      __attribute__((__aligned__(__alignof__(long long))));
  long double __clang_max_align_nonce2
      __attribute__((__aligned__(__alignof__(long double))));
} max_align_t;
#define __CLANG_MAX_ALIGN_T_DEFINED
#endif

#endif /* _CXX_COMPAT_H */
