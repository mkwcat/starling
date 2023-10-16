#if 0

#ifdef __cplusplus

#  include <bits/c++config.h>

// Normally libstdc++ requires configuration like this be done on library build
// time, but this workaround is fine as long as we're only using the headers,
// and we're not including any precompiled object files.

#  undef _GLIBCXX_HOSTED
#  define _GLIBCXX_HOSTED 0

#  undef _GLIBCXX_USE_C99_STDLIB
#  define _GLIBCXX_USE_C99_STDLIB 0

#endif

#endif
