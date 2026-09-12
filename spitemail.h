/* spitemail main program header
 * See spitemail.c for license information */

#ifndef SPITEMAIL_H
#define SPITEMAIL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Spite Mail is a Win32 application only */
#if defined _WIN32 || defined __MINGW32__
 #ifndef ON_WINDOWS
  #define ON_WINDOWS 1
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <io.h>
#else
#error This is a Win32 application.
#endif /* Win32 */

#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include "libjodycode.h"

/* Some types are different on Windows */
typedef uint64_t spitemail_ino_t;
typedef uint32_t spitemail_mode_t;

#define ISFLAG(a,b) ((a & b) == b)
#define SETFLAG(a,b) (a |= b)
#define CLEARFLAG(a,b) (a &= (~b))

/* Chunk sizing */
#ifndef CHUNK_SIZE
 #define CHUNK_SIZE 65536
#endif

/* Aggressive verbosity for deep debugging */
#ifdef LOUD_DEBUG
 #ifndef DEBUG
  #define DEBUG
 #endif
 #define LOUD(...) if ISFLAG(flags, F_LOUD) __VA_ARGS__
#else
 #define LOUD(a)
#endif

/* Compile out debugging stat counters unless requested */
#ifdef DEBUG
 #define DBG(a) a
#else
 #define DBG(a)
#endif


#ifdef __cplusplus
}
#endif

#endif /* SPITEMAIL_H */
