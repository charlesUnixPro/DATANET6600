//
//  dn6600_caf.c
//  dn6600
//
//  Created by Harry Reed on 12/31/15.
//
// Calculates word & character addresses for use bt dn6600
//

#include <stdio.h>
#include <stdbool.h>
#include "dn6600.h"
#include "dn6600_caf.h"


// some char position defines
#define W       0
#define DW      1   // double word
#define B_0     2
#define B_1     3
#define C_0     4
#define C_1     5
#define C_2     6
#define U_7     7   // canonical illegal access


static
struct  cinfo_t
{
    int size;   // size in bits of operand
    int mask1;  // mask data presented to processor/memory
    int mask2;  // mask for removing uninteresting bits
    int shift;  // L/R shift count to position bits correctly
} cinfo[8] = {
    { 18, 0777777,       0,  0 },   // 000 = single 18-bit word
    { 36,       0,       0,  0 },   // 001 = double (36-bit) word
    {  9,    0777, 0777000,  9 },   // 010 = 9 bit char 0, bits 0-8
    {  9,    0777, 0000777,  0 },   // 011 = 9 bit char 1. bits 9-17
    {  6,     077, 0770000, 12 },   // 100 = 6 bit char 0, bits 0-5
    {  6,     077, 0007700,  6 },   // 101 = 6 bit char 1, bits 6-11
    {  6,     077, 0000077,  0 },   // 110 = 6 bit char 2, bits 12-17
    {  0,       0,       0,  0 }    // 111 = illegal
};

word18 fromMemory(cpu_t *cpu, word15 addr, int charaddr)
{
    // we're reading from memory, so, we need look at charAddr to see what kind of data to present to the processor or IOM
    
    addr     &= BITS15; // keep word address to 15-bits
    charaddr &= BITS3;  // keep char addr to 0..7
    
    switch (charaddr)
    {
        case 0: // word addressing
            sim_debug(DBG_FINAL, &cpuDev, "Read Addr: %05o Data: %06o\n", 
                      addr, cpu->M[addr] & BITS18);
            return cpu->M[addr] & BITS18;
        case 1: // double-word addressing
            //        {
            //            // an odd address will use the pair (Y-1, Y)
            //            // an odd word is the least significant part of a double-precision number
            //            // an even address will use the pair (Y, Y+1)
            //            // an even word is the most significant part of a double-precision number
            //            // the memory location with the lower (even) address contains the most significant part of a double-word address
            //            word36 even = cpu->M[addr & 077776] & BITS18; // this will force an odd even (Y-1) and leave even alone (Y)
            //            word36 odd  = cpu->M[addr | 000001] & BITS18; // this will force an even odd (Y+1) and leave an odd alone (Y)
            //
            //            return even << 18LL | odd;
            //        }
        case 7:
            doFault(faultIllegalStore, "fromMemory(): illegal charaddr");
            return 0;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            break;
    }
    struct cinfo_t *ci = cinfo + charaddr;      // get mask & shifting info
    
    word18 data = cpu->M[addr] & BITS18;        // get 18-bits of data from memory
    
    data &= ci->mask2;                          // mask off unneeded bits
    
    data >>= ci->shift;                         // right justify
    
    sim_debug(DBG_FINAL, &cpuDev, "Read Addr: %05o Data: %06o\n", 
              addr, data & ci->mask1);
    return data & ci->mask1;                    // and present it to the CPU/IOM
}

word36 fromMemory36(cpu_t *cpu, word15 addr, int charaddr)
{
    // we're reading from memory, so, we need look at charAddr to see what kind of data to present to the processor or IOM
    
    addr     &= BITS15; // keep word address to 15-bits
    charaddr &= BITS3;  // keep char addr to 0..7
    
    switch (charaddr)
    {
        case 0: // word addressing
            sim_debug(DBG_FINAL, &cpuDev, "Read Addr: %05o Data: %06o\n", 
                      addr, cpu->M[addr] & BITS18);
            return cpu->M[addr] & BITS18;
        case 1: // double-word addressing
        {
            // an odd address will use the pair (Y-1, Y)
            // an odd word is the least significant part of a double-precision number
            // an even address will use the pair (Y, Y+1)
            // an even word is the most significant part of a double-precision number
            // the memory location with the lower (even) address contains the most significant part of a double-word address
            word36 even = cpu->M[addr & 077776] & BITS18; // this will force an odd even (Y-1) and leave even alone (Y)
            sim_debug(DBG_FINAL, &cpuDev, "Read Addr: %05o Data: %06o\n", 
                      addr & 077776, (word18)even);
            word36 odd  = cpu->M[addr | 000001] & BITS18; // this will force an even odd (Y+1) and leave an odd alone (Y)
            sim_debug(DBG_FINAL, &cpuDev, "Read Addr: %05o Data: %06o\n", 
                      addr | 000001, (word18)odd);

            return even << 18LL | odd;
        }
        case 7:
            doFault(faultIllegalStore, "fromMemory(): illegal charaddr");
            return 0;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            break;
    }
    struct cinfo_t *ci = cinfo + charaddr;      // get mask & shifting info
    
    word18 data = cpu->M[addr] & BITS18;        // get 18-bits of data from memory
    
    data &= ci->mask2;                          // mask off unneeded bits
    
    data >>= ci->shift;                         // right justify
    
    sim_debug(DBG_FINAL, &cpuDev, "Read Addr: %05o Data: %06o\n", 
              addr, data & ci->mask1);
    return data & ci->mask1;                    // and present it to the CPU/IOM
}

void toMemory(cpu_t *cpu, word18 data, word15 addr, word3 charaddr)
{
    // we're writing to memory, so, we need look at charaddr to see what kind of data to put where
    charaddr &= BITS3;

    switch (charaddr)
    {
        case 0:
            cpu->M[addr & BITS15] = data & BITS18;
            sim_debug(DBG_FINAL, &cpuDev, "Write Addr: %05o Data: %06o\n", 
                      addr & BITS15, data & BITS18);
            return;
        case 1: // double-word addressing
//        {
//            // an odd address will use the pair (Y-1, Y)
//            // an odd word is the least significant part of a double-precision number
//            // an even address will use the pair (Y, Y+1)
//            // an even word is the most significant part of a double-precision number
//            // the memory location with the lower (even) address contains the most significant part of a double-word address
//            word36 even = (data >> 18LL) & BITS18;
//            cpu->M[addr & 077776] = (word18)even; // this will force an odd even (Y-1) and leave even alone (Y)
//            word36 odd  =  data & BITS18;
//            cpu->M[addr | 000001] = (word18)odd; // this will force an even odd (Y+1) and leave an odd alone (Y)
//            return;
//        }
        case 7:
            doFault(faultIllegalStore, "to Memory(): illegal charaddr");
            return;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            break;
    }
    
    /*
     * Every character store is a RMW access ...
     */
    struct cinfo_t *ci = cinfo + charaddr;
    
    word18 oldM = cpu->M[addr] & BITS18;     // get 18-bits of data from memory
    
    data &= ci->mask1;                        // mask everything from incomming data but bits of interest

    data <<= ci->shift;                       // shift over by required amount

    word18 newM = (oldM & ~ci->mask2) | data; // mask out previous bits, 'or' in new data
    
    cpu->M[addr] = newM & BITS18;            // write out modified data back to memory
    sim_debug(DBG_FINAL, &cpuDev, "Write Addr: %05o Data: %06o\n", 
              addr, newM & BITS18);
}

void toMemory36(cpu_t *cpu, word36 data36, word15 addr, word3 charaddr)
{
    // we're writing to memory, so, we need look at charaddr to see what kind of data to put where
    charaddr &= BITS3;
    
    switch (charaddr)
    {
        case 0:
            cpu->M[addr & BITS15] = data36 & BITS18;
            sim_debug(DBG_FINAL, &cpuDev, "Write Addr: %05o Data: %06o\n", 
                      addr & BITS15, (word18) (data36 & BITS18));
            return;
        case 1: // double-word addressing
        {
            // an odd address will use the pair (Y-1, Y)
            // an odd word is the least significant part of a double-precision number
            // an even address will use the pair (Y, Y+1)
            // an even word is the most significant part of a double-precision number
            // the memory location with the lower (even) address contains the most significant part of a double-word address
            word36 even = (data36 >> 18LL) & BITS18;
            cpu->M[addr & 077776] = (word18)even; // this will force an odd even (Y-1) and leave even alone (Y)
            sim_debug(DBG_FINAL, &cpuDev, "Write Addr: %05o Data: %06o\n", 
                      addr & 077776, (word18)even);
            word36 odd  =  data36 & BITS18;
            cpu->M[addr | 000001] = (word18)odd;  // this will force an even odd (Y+1) and leave an odd alone (Y)
            sim_debug(DBG_FINAL, &cpuDev, "Write Addr: %05o Data: %06o\n", 
                      addr | 000001, (word18)odd);
            return;
        }
        case 7:
            doFault(faultIllegalStore, "to Memory(): illegal charaddr");
            return;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            break;
    }
    
    /*
     * Every character store is a RMW access ...
     */
    struct cinfo_t *ci = cinfo + charaddr;
    
    word18 oldM = cpu->M[addr] & BITS18;     // get 18-bits of data from memory
    
    word18 data18 = data36 &= ci->mask1;     // mask everything from incomming data but bits of interest
    
    data18 <<= ci->shift;                    // shift over by required amount
    
    word18 newM = (oldM & ~ci->mask2) | data18; // mask out previous bits, 'or' in new data
    
    cpu->M[addr] = newM & BITS18;            // write out modified data back to memory
    sim_debug(DBG_FINAL, &cpuDev, "Write Addr: %05o Data: %06o\n", 
              addr, newM & BITS18);
}

/*
 * Character Address Addition Rules matrix ... (dd01, pg. 3-20. Fig 3-3)
 */
static
int CAARmatrix[8][8] =
{
    { 0, 7, 7,   7, 7,   7,   7, 7 },
    { 7, 7, 7,   7, 7,   7,   7, 7 },
    { 7, 7, 2,   3, 7,   7 ,  7 ,7 },
    { 7, 7, 3, 012, 7,   7,   7, 7 },
    { 7, 7, 7,   7, 4,   5,   6, 7 },
    { 7, 7, 7,   7, 5,   6, 014, 7 },
    { 7, 7, 7,   7, 6, 014, 015, 7 },
    { 7, 7, 7,   7, 7,   7,   7, 7 }
};

/*
 * Add 2 addresses together (both word address assumed to be sign-extended to 32-bits)
 */
bool addAddr32(int wx, word3 cx, int wy, word3 cy, word15 *wz, word3 *cz)
{
    word15 w = wx + wy;  // calculate word address (ignoring carry/overflow for now)
    
    word3 c = CAARmatrix[cx & BITS3][cy & BITS3]; // determine character fractional address
    
    if (c == 7)
    {
        doFault(faultIllegalStore, "addAddr32(): illegal charAddr");
        return false;
    }
    if (c & 010)  // add FA carry to word address if necessary
        w += 1;

    w &= BITS15;  // return only 15-bits for address
    
    *wz = w;      // return word address
    *cz = c;      // return char address
    return true;
}

/*
 * get w/c addresses given by T
 */
static
void
getT(cpu_t *cpu, word2 t, word15 rICorZero, word3 *c, word15 *w)
{
    switch (t)
    {
        case 0:     // typically rIC or 0
            *w = _W(rICorZero);
            *c = W;    // not assigned.
            return;
        case 1:     // X1
            *w = _W(cpu->rX1);
            *c = _C(cpu->rX1);
            return;
        case 2:     // X2
            *w = _W(cpu->rX2);
            *c = _C(cpu->rX2);
            return;
        case 3:     // X3
            *w = _W(cpu->rX3);
            *c = _C(cpu->rX3);
            return;
    }
}

/*
 * returns final word and character addresses used by memory reference instructions
 */
bool doCAF(cpu_t *cpu, bool i, word2 t, word9 d, word15 *w, word3 *c)
{
    sim_debug(DBG_CAF, &cpuDev, "CAF entry: I %d T %o D %03o\n",
              i ? 1 : 0, t, d);
    word3 ct = 0;                               // default to word addressing
    word15 wt;
    
    if (t == 0) {                               // word addressing
        wt = (SIGNEXT9(d & BITS9) + cpu->rIC) & BITS15;    // word displacement is all 9-bits of displacement field
        sim_debug(DBG_CAF, &cpuDev, "word addressing; wt %05o\n", wt);
    }
    else
    {                                           // possible char addressing
        getT(cpu, t, cpu->rIC, &ct, &wt);       // fetch T modification
        int32_t w6 = SIGNEXT6(d & BITS6);       // word displacement is lower 6-bits of 9-bit displacement field
        sim_debug(DBG_CAF, &cpuDev, "potential non-word addressing; w6 %05o\n", w6);
        word3 c6 = (d >> 6) & BITS3;            // char address is upper 3-bits of 9-bit displacement field
        if (addAddr32(wt, ct, w6, c6, &wt, &ct) == false)
            return false;                       // some error occured
        if (!(ct == 0 && i))                    // is a word access + indirecttion cycle required?
        {
            *c = ct;
            *w = wt;
            return true;
        }
    }
    
    word15 Y = wt;                              // word address is in Y
                                                // char address in in ct
    while (i)                                   // indirect addressing
    {
        word18 CY = cpu->M[Y];                  // so fetch indirect word @ addr Y
        t = _T(CY);                             // extract T field
        if (t == 0)                             // word addressing
        {
            Y = CY & BITS15;                    // address is all 15-bits of Y
            ct = t;                             // and ct is word-addressing
        } else {
            getT(cpu, t, 0, &ct, &wt);          // fetch T Address
            word12 w12 = SIGNEXT12(CY & BITS12);// get 12-bit displacement
            word3 c12 = (CY >> 12) & BITS3;
            if (addAddr32(wt, ct, w12, c12, &Y, &ct) == false)
                return false;                   // some error occured
        }
        i = _I(CY);                             // more indirection?
        
        // XXX if we want an indirection stall alarm, this is where to put the counter...
    }
    
    *c = ct;
    *w = Y;
    sim_debug(DBG_CAF, &cpuDev, "CAF Exit C %o W %05o\n", ct, Y);
    return true;    // everything hunky-dorey
}


/*
 * return data for I/T/D style addressing ...
 */
word18 readITD(cpu_t *cpu, bool i, word2 t, word9 d)
{
    word3 c = 0;
    word15 w = 0;
    
    bool bOK = doCAF(cpu, i, t, d, &w, &c);
    if (!bOK)
        return false;
    
    return fromMemory(cpu, w, c);
}
word36 readITD36(cpu_t *cpu, bool i, word2 t, word9 d)
{
    word3 c = 0;
    word15 w = 0;
    
    bool bOK = doCAF(cpu, i, t, d, &w, &c);
    if (!bOK)
        return false;
    
    return fromMemory36(cpu, w, c);
}
/*
 * write data for I/T/D style addressing ...
 */
void writeITD(cpu_t *cpu, word18 data, bool i, word2 t, word9 d)
{
    word3 c = 0;
    word15 w = 0;
    
    bool bOK = doCAF(cpu, i, t, d, &w, &c);
    if (!bOK)
        return;

    toMemory(cpu, data, w, c);
}
void writeITD36(cpu_t *cpu, word36 data, bool i, word2 t, word9 d)
{
    word3 c = 0;
    word15 w = 0;
    
    bool bOK = doCAF(cpu, i, t, d, &w, &c);
    if (!bOK)
        return;
    
    toMemory36(cpu, data, w, c);
}
