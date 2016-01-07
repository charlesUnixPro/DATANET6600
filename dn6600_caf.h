//
//  dn6600_caf.h
//  dn6600
//
//  Created by Harry Reed on 12/31/15.
//
// header file to help support dn6600 enulator
//

#ifndef dn6600_caf_h
#define dn6600_caf_h

#include <stdio.h>
#include <stdbool.h>

typedef uint32_t word1;
typedef uint32_t word2;
typedef uint32_t word3;
typedef uint32_t word6;
typedef uint32_t word9;
typedef uint32_t word12;
typedef uint32_t word15;
typedef uint32_t word18;
typedef uint64_t word36;

#define illmemop 00445

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

#define MemSize 32367

typedef struct
{
    word18 M[MemSize];      // Our memory store. 32K x 18. Oooh, that's at least 589,806 cores (not incl parity/ecc)
    
    word18  rIR;            // indicator register
    
    word18  rA;             // accumulator A
    word18  rQ;             // accumulator Q
    word15  rIC;            // 15-bit instruction counter
    word18  rX1;            // index register X1
    word18  rX2;            // index register X2
    word18  rX3;            // index register X3
} cpu_t;

// some char position defines
#define W       0
#define DW      1   // double word
#define B_0     2
#define B_1     3
#define C_0     4
#define C_1     5
#define C_2     6
#define U_7     7   // canonical illegal access


word18 fromMemory  (cpu_t *cpu, word15 addr, int charAddr);
word36 fromMemory36(cpu_t *cpu, word15 addr, int charaddr);

void toMemory  (cpu_t *cpu, word18 data, word15 addr, word3 charaddr);
void toMemory36(cpu_t *cpu, word36 data, word15 addr, word3 charaddr);

bool doCAF(cpu_t *cpu, bool i, word2 t, word9 d, word2 *c, word18 *w);

word18 readITD  (cpu_t *cpu, bool i, word2 t, word9 d);
word36 readITD36(cpu_t *cpu, bool i, word2 t, word9 d);

void writeITD  (cpu_t *cpu, word18 data, bool i, word2 t, word9 d);
void writeITD36(cpu_t *cpu, word36 data, bool i, word2 t, word9 d);


#endif /* dn6600_caf_h */
