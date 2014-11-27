#ifndef _CXX_COMPAT_H
#define _CXX_COMPAT_H

//C++11 compatibility stuff
#define container_of(ptr, type, member)   \
    ((type *) ((char *) (ptr) - offsetof(type, member)))

#define register

#endif /* _CXX_COMPAT_H */
