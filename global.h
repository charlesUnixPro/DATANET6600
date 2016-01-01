#include "sim_defs.h"

#ifdef __GNUC__
#define NO_RETURN   __attribute__ ((noreturn))
#define UNUSED      __attribute__ ((unused))
#else
#define NO_RETURN
#define UNUSED
#endif

typedef uint word1;
typedef uint word2;
typedef uint word3;
typedef uint word6;
typedef uint word9;
typedef uint word15;
typedef uint word18;

// Word size
enum { WSZ = 18 };

// Address size
enum { ASZ = 15 };

// Memory size
enum { MEM_SIZE = 1 << 15 };

#define DBG_DEBUG       (1U << 0)
#define DBG_TRACE       (1U << 1)

