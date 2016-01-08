#include <stdint.h>
#include "sim_defs.h"

#ifdef __GNUC__
#define NO_RETURN   __attribute__ ((noreturn))
#define UNUSED      __attribute__ ((unused))
#else
#define NO_RETURN
#define UNUSED
#endif

typedef uint32_t word1;
typedef uint32_t word2;
typedef uint32_t word3;
typedef uint32_t word6;
typedef uint32_t word9;
typedef uint32_t word12;
typedef uint32_t word15;
typedef uint32_t word18;
typedef uint64_t word36;

#define BIT0    ((word18)0400000)

#define BITS2   ((word2) 03)
#define BITS3   ((word3) 07)
#define BITS6   ((word6) 077)
#define BITS9   ((word9) 0777)
#define BITS12  ((word12)07777)
#define BITS15  ((word15)077777)
#define BITS18  ((word18)0777777)


#define SIGN6            040        // represents sign bit of a 9-bit 2-comp number
#define SGNX6   037777777740        // sign extend a 9-bit number to 32-bits
#define SIGNEXT6(x)    (((x) & SIGN6) ? ((x) | SGNX6) : (x))

#define SIGN9           0400        // represents sign bit of a 9-bit 2-comp number
#define SGNX9   037777777400        // sign extend a 9-bit number to 32-bits
#define SIGNEXT9(x)    (((x) & SIGN9) ? ((x) | SGNX9) : (x))

#define SIGN12         04000        // represents sign bit of a 12-bit 2-comp number
#define SGNX12  037777774000        // sign extend a 12-bit number to 32-bits
#define SIGNEXT12(x)    (((x) & SIGN12) ? ((x) | SGNX12) : (x))

//#define SIGN14        020000        // represents sign bit of a 14-bit 2-comp number
//#define SGNX14  037777760000        // sign extend a 14-bit number to 32-bits
//#define SIGNEXT14(x)    (((x) & SIGN14) ? ((x) | SGNX14) : (x))

//#define SIGN15        040000        // represents sign bit of a 15-bit 2-comp number
//#define SGNX15  037777740000        // sign extend a 15-bit number to 32-bits
//#define SIGNEXT15(x)    (((x) & SIGN15) ? ((x) | SGNX15) : (x))

#define _I(x)    (((x) & BIT0) ? true : false)   // extract indirect bit
#define _T(x)    (((x) >> 15) & BITS2)           // extract T field
#define _C(x)    (((x) >> 15) & BITS3)           // extract C (char address) from 18-bit word
#define _W(x)     ((x) & BITS15)                 // extract W (word address) from 18-bit word

// Word size
enum { WSZ = 18 };

// Address size
enum { ASZ = 15 };

// Memory size
enum { MEM_SIZE = 1 << 15 };

typedef struct
{
    word18 M[MEM_SIZE];      // Our memory store. 32K x 18. Oooh, that's at least 589,806 cores (not incl parity/ecc)
    
    word18  rIR;            // indicator register
    
    word18  rA;             // accumulator A
    word18  rQ;             // accumulator Q
    word15  rIC;            // 15-bit instruction counter
    word18  rX1;            // index register X1
    word18  rX2;            // index register X2
    word18  rX3;            // index register X3
    word18  rS;             // I/O channel select register
    word1   rII;            // interrupt inhibit

} cpu_t;

extern cpu_t cpu;

// some char position defines
#define W       0
#define DW      1   // double word
#define B_0     2
#define B_1     3
#define C_0     4
#define C_1     5
#define C_2     6
#define U_7     7   // canonical illegal access

// FAULTS

// Fault vector memory locations
enum 
  {
    faultPowerShutdownBeginning = 0440,
    faultRestart =                0441,
    faultParity =                 0442,
    faultIllegalOpcode =          0443,
    faultOverflow =               0444,
    faultIllegalStore =           0445,
    faultDivideCheck =            0446,
    faultIllegalProgramInt =      0447
  };

#define DBG_DEBUG       (1U << 0)
#define DBG_TRACE       (1U << 1)

void doFault (int f, const char * msg) NO_RETURN;
