#include <unistd.h>
#include <stdbool.h>

#include "dn6600.h"
#include "dn6600_caf.h"
#include "coupler.h"

char sim_name [] = "dn6600";
int32 sim_emax = 1;
// DEVICE *sim_devices[] table of pointers to simulated devices, NULL terminated
// char *sim_stop_messages[] table of pointers to error messages
// t_stat sim_load (…) binary loader subroutine
// t_stat parse_sym (…) symbolic instruction parse subroutine
// t_stat fprint_sym (…) symbolic instruction print subroutine 
// void (*sim_vm_init) (void) pointer to once-only initialization routine for VM
// t_addr (*sim_vm_parse_addr) (…) pointer to address parsing routine
// void (*sim_vm_fprint_addr) (…) pointer to address output routine
// char (*sim_vm_read) (…) pointer to command input routine
// void (*sim_vm_post) (…) pointer to command post-processing routine
// CTAB *sim_vm_cmd pointer to simulator-specific command table


// Memory

// 18 bit words, 18 bit Addresses

// INSTRUCTION FORMAT (dd01, pg 304).
//
// Memory Reference Instructions
//
//                                           1   1   1   1   1   1   1   1
//   0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5   6   7
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// | I |   T   |    OPCODE             |      D                            |
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//
// I      Indirect bit: when on, the effective address is computed from the 
//        inidrect word.
// T      Tag Field: specifies address modifcation using an index register (X1,
//        X2, X3, or the instruction counter (IC).
// Opcode Operation code.
// D      Displacement.
//

// The basic method of forming effective addresses consistes of adding the the
// 9-but displacement field (D) to the complete address obtained from one of
// the three index registers of the instruction counter. The displacement (D) 
// is treated as a 9-bit number in 2's complement form. When indirect 
// addressing is used as the address of an indirect word with the following
// format:
//
//                                           1   1   1   1   1   1   1   1
//   0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5   6   7
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// | I |   T   |    Y                                                      |
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//
// This differs from the instruction word, in that Y is an address field
// rather the a displacement field, and no base address is needed to form a
// a full 15-bit address. The I specifies further indirect addressing. [CAC: 
// what about T?]
//
//
// Nonmemory reference instructions
//
// Group 1
//
//                                           1   1   1   1   1   1   1   1
//   0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5   6   7
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// |     S1    |    OPCODE             |      D                            |
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//
//  S1   Suboperation code
//
// Group 2
//                                           1   1   1   1   1   1   1   1
//   0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5   6   7
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// |     S1    |    OPCODE             |     S2    |     K                 |
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//
// S1, S2   Suboperation codes; These two code form a prefix and a suffix
//          to the operation codes and are used to determine the instruction
//          within the group.
// K        Operation value: This field is used for such functions as shift
//          counts.

cpu_t cpu;

static REG cpu_reg [] =
  {
    {ORDATA (IC, cpu . rIC,    ASZ), 0, 0}, // Must be first, per simh
    {ORDATA (X1, cpu . rX1,    WSZ), 0, 0},
    {ORDATA (X2, cpu . rX2,    WSZ), 0, 0},
    {ORDATA (X3, cpu . rX3,    WSZ), 0, 0},
    {ORDATA (A,  cpu . rA,     WSZ), 0, 0},
    {ORDATA (Q,  cpu . rQ,     WSZ), 0, 0},
    {ORDATA (IR, cpu . rIR,    8),   0, 0}, // Indicator register
    {ORDATA (S,  cpu . rS,     6),   0, 0}, // I/O channel select register
    {ORDATA (II, cpu . rII,    1),   0, 0}, // interrupt inhibit
    NULL
  };

REG * sim_PC = & cpu_reg [0];

// Indicator register (as defined by the LDI/STI instructions (DD01, pg 3-12)

//   bit 
// position    indicator
//
//    0        Zero
//    1        Negative
//    2        Carry
//    3        Overflow
//    4        Interrupt inhibit
//    5        Parity fault inhibit
//    6        Overflow fault inhibit
//    7        Parity error
//

static UNIT cpu_unit =
  {
    UDATA (NULL, UNIT_FIX|UNIT_BINK, MEM_SIZE), 0, 0, 0, 0, 0, NULL, NULL
  };

/* scp Debug flags */

static DEBTAB cpu_dt[] = 
  {
    { "TRACE",      DBG_TRACE       },
    { NULL,         0               }
  };

static DEVICE cpuDev =
  {
    "CPU",          /* name */
    & cpu_unit,     /* units */
    cpu_reg,        /* registers */
    NULL /*&cpu_mod*/,        /* modifiers */
    1,              /* #units */
    8,              /* address radix */
    ASZ,            /* address width */
    1,              /* addr increment */
    8,              /* data radix */
    WSZ,            /* data width */
    NULL /*&cpu_ex*/,        /* examine routine */
    NULL /*&cpu_dep*/,       /*!< deposit routine */
    NULL /*&cpu_reset*/,     /*!< reset routine */
    NULL /*&cpu_boot*/,           /*!< boot routine */
    NULL,           /* attach routine */
    NULL,           /* detach routine */
    NULL,           /* context */
    DEV_DEBUG,      /* device flags */
    0,              /* debug control flags */
    cpu_dt,         /* debug flag names */
    NULL,           /* memory size change */
    NULL,           /* logical name */
    NULL,           // help
    NULL,           // attach help
    NULL,           // help context
    NULL            // description
  };

// ADDRESS FORMATION

//                                           1   1   1   1   1   1   1   1
//   0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5   6   7
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// |     CY    |       WY                                                  |
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//
//  CY   character address
//  WY   word address
//
//    Fractional       CY
//  Interpretation   Value    Character addressed
//  --------------   -----    -------------------
//     0/3            100     6-bit character number 0 (bits 0-5)
//     1/3            101     6-bit character number 1 (bits 6-11)
//     2/3            110     6-bit character number 2 (bits 12-17)
//     0/2            010     9-bit character number 0 (bits 0-8)
//     1/2            011     9-bit character number 1 (bits 9-17)
//   Full word        000     Full word
//
// When characters are address, they are transfered to and from memory 
// right justified. Unused bit are zeros for operations from memory and are
// ignored for operations to memory.
//
// An example of load and store upon 6-bit character No. 0 (CY = 100):
//
// Memory to processor or IOM:
//                                           1   1   1   1   1   1   1   1
//   0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5   6   7
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// |   CHARACTER           |  ignored...                                   |
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// \                      /
//  -----------v----------
//             |
//             +-----------------------------------------------+
//                                                             |
//                                                             ^
//                                                  ----------------------
//                                                 /                      \\
//                                           1   1   1   1   1   1   1   1
//   0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5   6   7
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// |   Zeros                                       |   CHARACTER           |
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//
// Processor or IOM to memory:
//
//                                           1   1   1   1   1   1   1   1
//   0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5   6   7
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// |   Ignored                                     |   CHARACTER           |
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//                                                  \                      /
//                                                   -----------v----------
//                                                              |
//             +-----------------------------------------------+
//             |
//             ^
//   ----------------------
//  /                      \\
//                                           1   1   1   1   1   1   1   1
//   0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5   6   7
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// |  CHARACTER            |   Unchanged                                   |
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+



// BASIC EFFECTIVE ADDRESS FORMATION
//
//  -------------------------------------------------------------------
//  |       | Instruction word            | Indirect word             |
//  |       -----------------------------------------------------------
//  |       |  I=0         |  I=1         |  I=9       |  I=1         |
//  -------------------------------------------------------------------
//  | T=00  |  Y**=IC+D    |  Y*=C(IC+D)  |  Y**=Y     |  Y*=C(Y)     |
//  | T=01  |  Y**=X1+D    |  Y*=C(X1+D)  |  Y**=X1+Y  |  Y*=C(X1+Y)  |
//  | T=02  |  Y**=X2+D    |  Y*=C(X2+D)  |  Y**=X2+Y  |  Y*=C(X2+Y)  |
//  | T=03  |  Y**=X3+D    |  Y*=C(X3+D)  |  Y**=X3+Y  |  Y*=C(X3+Y)  |
//  -------------------------------------------------------------------

DEVICE * sim_devices [] =
  {
    & cpuDev,
    & couplerDev
  };

const char * sim_stop_messages [] =
  {
// XXX
  };

t_stat sim_load (FILE * fileref, char * cptr, char * fnam, int flag)
  {
// XXX
    return SCPE_UNK;
  }

//Based on the switch variable, parse character string cptr for a symbolic 
// value val at the specified addr in unit uptr.

t_stat parse_sym (UNUSED char * cptr, UNUSED t_addr addr, UNUSED UNIT * uptr,
                  UNUSED t_value * val, UNUSED int32 sswitch)
{
// XXX
    return SCPE_ARG;
}

// simh "fprint_sym" – Based on the switch variable, symbolically output to 
// stream ofile the data in array val at the specified addr in unit uptr.

t_stat fprint_sym (FILE * ofile, UNUSED t_addr  addr, t_value *val,
                   UNIT *uptr, int32 sw)
{
// XXX
    return SCPE_ARG;
}

static inline word18 getbits18 (word18 x, uint i, uint n)
  {
    // bit 17 is right end, bit zero is 18th from the right
    int shift = 17 - (int) i - (int) n + 1;
    if (shift < 0 || shift > 17)
      {
        sim_printf ("getbits18: bad args (%06o,i=%d,n=%d)\n", x, i, n);
        return 0;
      }
     else
      return (x >> (unsigned) shift) & ~ (~0U << n);
  }

#define MASKBITS(x) (~(~((uint)0) << x))   // lower (x) bits all ones

static inline word18 setbits18 (word18 x, uint p, uint n, word18 val)
  {
    int shift = 18 - (int) p - (int) n;
    if (shift < 0 || shift > 17)
      {
        sim_printf ("setbits18: bad args (%06o,pos=%d,n=%d)\n", x, p, n);
        return 0;
      }
    word18 mask = ~ (~0U << n);  // n low bits on
    mask <<= (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    word18 result = (x & ~ mask) | ((val & MASKBITS (n)) << (18 - p - n));
    return result;
  }

static inline void putbits18 (word18 * x, uint p, uint n, word18 val)
  {
    int shift = 18 - (int) p - (int) n;
    if (shift < 0 || shift > 17)
      {
        sim_printf ("putbits18: bad args (%06o,pos=%d,n=%d)\n", * x, p, n);
        return;
      }
    word18 mask = ~ (~0U << n);  // n low bits on
    mask <<= (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    * x = (* x & ~mask) | ((val & MASKBITS (n)) << (18 - p - n));
    return;
  }

struct opc_t
  {
    char * name;
    // opcG1a: 73
    // opcG1b: 22
    // opcG1c: 52
    // opcG1d: 12
    // opcG2:  33
    enum { opcILL, opcMR, opcG1, opcG2 } grp; // opcode group (memory, group1, group2)
// prepare address is implied by grp == opcMR
// opcRD operand read
// oprWR operand write
    bool opRD, opWR;
    enum { opW, opDW } opSize;

#define OP_CA    false, false, opW
#define OP_RD    true,  false, opW
#define OP_WR    false, true,  opW
#define OP_RMW   true,  true,  opW
#define OP_DCA   false, false, opDW
#define OP_DRD   true,  false, opDW
#define OP_DWR   false, true,  opDW
#define OP_DRMW  true,  true,  opDW
#define OP_NULL  false, false, 0
  };

static struct opc_t opcTable [64] =
  {
// 00 - 07
    { "ill",     opcILL, OP_NULL   }, // 00
    { "MPF",     opcMR,  OP_RD     }, // 01 Multiply fraction
    { "ADCX2",   opcMR,  OP_RD     }, // 02
    { "LDX2",    opcMR,  OP_RD     }, // 03
    { "LDAQ",    opcMR,  OP_DRD    }, // 04
    { "ill",     opcILL, OP_NULL   }, // 05
    { "ADA",     opcMR,  OP_RD     }, // 06
    { "LDA",     opcMR,  OP_RD     }, // 07

// 10 - 17
    { "TSY",     opcMR,  OP_WR     }, // 10
    { "ill",     opcILL, OP_NULL   }, // 11
    { "grp1d",   opcG1 }, // 12
    { "STX2",    opcMR,  OP_WR     }, // 13
    { "STAQ",    opcMR,  OP_DWR    }, // 14
    { "ADAQ",    opcMR,  OP_DRD    }, // 15
    { "ASA",     opcMR,  OP_RMW    }, // 16
    { "STA",     opcMR,  OP_WR     }, // 17

// 20 - 27
    { "SZN",     opcMR,  OP_RD     }, // 20
    { "DVF",     opcMR,  OP_RD     }, // 21
    { "grp1b",   opcG1 }, // 22
    { "CMPX2",   opcMR,  OP_RD     }, // 23
    { "SBAQ",    opcMR,  OP_DRD    }, // 24
    { "ill",     opcILL, OP_NULL   }, // 25
    { "SBA",     opcMR,  OP_RD     }, // 26
    { "CMPA",    opcMR,  OP_RD     }, // 27

// 30 - 37
    { "LDEX",    opcMR,  OP_NULL   }, // 30
    { "CANA",    opcMR,  OP_RD     }, // 31
    { "ANSA",    opcMR,  OP_RMW    }, // 32
    { "grp2",    opcG2  }, // 32
    { "ANA",     opcMR,  OP_RD     }, // 34
    { "ERA",     opcMR,  OP_RD     }, // 35
    { "SSA",     opcMR,  OP_RMW    }, // 36
    { "ORA",     opcMR,  OP_RD     }, // 37

// 40 - 47
    { "ADCX3",   opcMR,  OP_RD     }, // 40
    { "LDX3",    opcMR,  OP_RD     }, // 41
    { "ADCX1",   opcMR,  OP_RD     }, // 42
    { "LDX1",    opcMR,  OP_RD     }, // 43
    { "LDI",     opcMR,  OP_RD     }, // 44
    { "TNC",     opcMR,  OP_NULL   }, // 45
    { "ADQ",     opcMR,  OP_RD     }, // 46
    { "LDQ",     opcMR,  OP_RD     }, // 47

// 50 - 57
    { "STX3",    opcMR,  OP_WR     }, // 50
    { "ill",     opcILL, OP_NULL   }, // 51
    { "grp1c",   opcG1 }, // 52
    { "STX1",    opcMR,  OP_WR     }, // 53
    { "STI",     opcMR,  OP_WR     }, // 54
    { "TOV",     opcMR,  OP_NULL   }, // 55
    { "STZ",     opcMR,  OP_WR     }, // 56
    { "STQ",     opcMR,  OP_WR     }, // 57

// 60 - 67
    { "CIOC",    opcMR,  OP_NULL   }, // 60
    { "CMPX3",   opcMR,  OP_RD     }, // 61
    { "ERSA",    opcMR,  OP_RMW    }, // 62
    { "CMPX1",   opcMR,  OP_RD     }, // 63
    { "TNZ",     opcMR,  OP_NULL   }, // 64
    { "TPL",     opcMR,  OP_NULL   }, // 65
    { "SBQ",     opcMR,  OP_RD     }, // 66
    { "CMPQ",    opcMR,  OP_RD     }, // 67

// 70 - 77
    { "STEX",    opcMR,  OP_NULL   }, // 70
    { "TRA",     opcMR,  OP_NULL   }, // 71
    { "ORSA",    opcMR,  OP_RMW    }, // 72
    { "grp1a",   opcG1 }, // 73
    { "TZE",     opcMR,  OP_NULL   }, // 74
    { "TMI",     opcMR,  OP_NULL   }, // 75
    { "AOS",     opcMR,  OP_RMW    }, // 76
    { "ill",     opcILL, OP_NULL   } // 77

  };

static char * disassemble (word18 ins)
  {
    static char result[132] = "???";
    word6 OPCODE = getbits18 (ins, 3, 6);
    strcpy (result, opcTable [OPCODE] . name);
    return result;
  }

void doFault (int f, const char * msg)
  {
    fprintf(stderr, "fault %05o : %s\n", f, msg);
    // more later
    exit (1);
  }

static void doUnimp (word6 opc) NO_RETURN;

static void doUnimp (word6 opc)
  {
    sim_printf ("unimplemented %02o\n", opc);
    // more later
    exit (1);
  }

t_stat sim_instr (void)
  {
    int reason = 0;
    do
      {
        if (sim_interval <= 0)
          {
            if ((reason = sim_process_event ()))
              break;
          }

        // Check for outstanding interrupts and process if required. 

        // ... XXX

        // Check for other processor-unique events, such as wait-state
        // outstanding or traps outstanding. 

        // ... XXX

        // Check for an instruction breakpoint. SCP has a comprehensive
        // breakpoint facility. It allows a VM to define many different kinds
        // of breakpoints. The VM checks for execution (type E) breakpoints
        // during instruction fetch. 

        // ... XXX

        // Fetch the next instruction, increment the PC, optionally decode the
        // address, and dispatch (via a switch statement) for execution.        

        word18 ins = cpu . M [cpu . rIC];
        sim_debug (DBG_TRACE, & cpuDev, "%05o %06o %s\n", cpu . rIC, ins, disassemble (ins));

        word6 OPCODE = getbits18 (ins, 3, 6);
        word1 I = 0;
        word2 T = 0;
        word9 D = 0;
        word3 S1 = 0;
        word3 S2 = 0;
        word6 K = 0;
        word15 W = 0;
        word3 C = 0;
        word18 Y = 0;
        word36 YY = 0;

        switch (opcTable [OPCODE] . grp)
          {
            case opcILL:
              {
                // XXX fault illegal opcode
                break;
              }

            case opcMR:
              {
                I = getbits18 (ins, 0, 1);
                T = getbits18 (ins, 1, 2);
                D = getbits18 (ins, 9, 9);
                doCAF (& cpu, I, T, D, & W, & C);
                if (opcTable [OPCODE] . opRD)
                  {
                    if (opcTable [OPCODE] . opSize == opW)
                      {
                        Y = fromMemory (& cpu, W, C);
                      }
                    else if (opcTable [OPCODE] . opSize == opDW)
                      {
                        YY = fromMemory36 (& cpu, W, C);
                      }
                  }
                break;
              }

            case opcG1:
              {
                S1 = getbits18 (ins, 0, 3);
                D = getbits18 (ins, 9, 9);
                break;
              }
            case opcG2:
              {
                S1 = getbits18 (ins, 0, 3);
                S2 = getbits18 (ins, 9, 3);
                K = getbits18 (ins, 12, 6);
                break;
              }
          }

#define ILL doFault (faultIllegalOpcode, "illegal opcode")
#define UNIMP doUnimp (OPCODE);

        switch (OPCODE)
          {
            case 000: // illegal
              ILL;

            case 001: // MPF
              UNIMP;

            case 002: // ADCX2
              UNIMP;

            case 003: // LDX2
              UNIMP;

            case 004: // LDAQ
              // Load AQ
              cpu . rA = (YY >> 18) & BITS18;
              cpu . rQ = (YY >>  0) & BITS18;
              break;

            case 005: // ill
              ILL;

            case 006: // ADA
              UNIMP;

            case 007: // LDA
              UNIMP;


// 10 - 17
            case 010: // TSY
              UNIMP;

            case 011: // ill
              ILL;

            case 012: // grp1d
              {
                switch (S1)
                  {
                    case 0:  // RIER
                      UNIMP;

                    case 4:  // RIA
                      UNIMP;

                    default:
                      ILL;
                  } // switch (S1)
              }
              break;

            case 013: // STX2
              UNIMP;

            case 014: // STAQ
              UNIMP;

            case 015: // ADAQ
              UNIMP;

            case 016: // ASA
              UNIMP;

            case 017: // STA
              UNIMP;


// 20 - 27
            case 020: // SZN
              UNIMP;

            case 021: // DVF
              UNIMP;

            case 022: // grp1b
              {
                switch (S1)
                  {
                    case 0:  // IANA
                      UNIMP;

                    case 1:  // IORA
                      UNIMP;

                    case 2:  // ICANA
                      UNIMP;

                    case 3:  // IERA
                      UNIMP;

                    case 4:  // ICMPA
                      UNIMP;

                    default:
                      ILL;
                  } // switch (S1)
              }
              break;

            case 023: // CMPX2
              UNIMP;

            case 024: // SBAQ
              UNIMP;

            case 025: // ill
              ILL;

            case 026: // SBA
              UNIMP;

            case 027: // CMPA
              UNIMP;


// 30 - 37
            case 030: // LDEX
              UNIMP;

            case 031: // CANA
              UNIMP;

            case 032: // ANSA
              UNIMP;

            case 033: // grp2
              {
                switch (S1)
                  {
                    case 0:
                      {
                        switch (S2)
                          {
                            case 2: // CAX2
                              // Copy A into X2
                              cpu . rX2 = cpu . rA;
                              break;

                            case 4: // LLS
                             UNIMP;

                            case 5: // LRS
                             UNIMP;

                            case 6: // ALS
                             UNIMP;

                            case 7: // ARS
                             UNIMP;

                            default:
                              ILL;
                          } // switch (S2)
                      }
                      break;

                    case 1:
                      {
                        switch (S2)
                          {
                            case 4: // NRML
                             UNIMP;

                            case 6: // NRM
                             UNIMP;

                            default:
                              ILL;
                          } // switch (S2)
                      }
                      break;

                    case 2:
                      {
                        switch (S2)
                          {
                            case 1: // NOP
                              // No Operation
                              break;

                            case 2: // CX1A
                              // Copy X1 into A
                              cpu . rA = cpu . rX1;
                              break;

                            case 4: // LLR
                              UNIMP;

                            case 5: // LRL
                              UNIMP;

                            case 6: // ALR
                              UNIMP;

                            case 7: // ARL
                              UNIMP;

                            default:
                              ILL;
                          } // switch (S2)
                      }
                      break;

                    case 3:
                      {
                        switch (S2)
                          {
                            case 1: // INH
                              // Interrupt inhibit
                              // Interrupt inhibit indicator is turned ON
                              cpu . rII = 1;
                              break;

                            case 2: // CX2A
                              // Copy X2 into A
                              cpu . rA = cpu . rX2;
                              break;

                            case 3: // CX3A // XXX this may be a dd01 typo 2,33,3 fits the pattern
                              // Copy X3 into A
                              cpu . rA = cpu . rX3;
                              break;

                            case 6: // ALP
                              UNIMP;

                            default:
                              ILL;
                          } // switch (S2)
                      }
                      break;

                    case 4:
                      {
                        switch (S2)
                          {
                            case 1: // DIS
                             UNIMP;

                            case 2: // CAX1
                              // Copy A into X1
                              cpu . rX1 = cpu . rA;
                              break;

                            case 3: // CAX3
                              // Copy A into X3
                              cpu . rX3 = cpu . rA;
                              break;

                            case 6: // QLS
                             UNIMP;

                            case 7: // QRS
                              UNIMP;

                            default:
                              ILL;
                          } // switch (S2)
                      }
                      break;

                    case 6:
                      {
                        switch (S2)
                          {
                            case 3: // CAQ
                              // Copy A into Q
                              cpu . rQ = cpu . rA;
                              break;

                            case 6: // QLR
                              UNIMP;

                            case 7: // QRL
                              UNIMP;

                            default:
                              ILL;
                          } // switch (S2)
                      }
                      break;

                    case 7:
                      {
                        switch (S2)
                          {
                            case 1: // ENI
                              // Enable interrupt
                              // Interrupt inhibit indicator is turned OFF
                              cpu . rII = 0;
                              break;

                            case 3: // CQA
                              // Copy Q into A
                              cpu . rA = cpu . rQ;
                              break;

                            case 6: // QLP
                              // Q Left Parity Rotate
                              // Rotate C(Q) by Y (12-17) positions, enter each
                              // bit leaving position zero into position 17.
                              // Zero: If the number of 1's leavong position 0
                              // is even, then ON; otherwise OFF
                              // Negative: If (C(Q)0 = 1, then ON; otherwise OFF
                              
                              {
                                int ones = 0;
                                for (uint n = 0; n < K; n ++)
                                  {
                                    word1 out = getbits18 (cpu . rQ, 0, 1);
                                    cpu . rQ <<= 1;
                                    cpu . rQ |= out;
                                    if (out)
                                      ones ++;
                                  }
                                SCF (ones % 2 == 0, cpu . rIR, I_ZERO);
                                SCF (getbits18 (cpu . rQ, 0, 1) == 1, cpu . rIR, I_NEG);
                              }
                              break;

                            default:
                              ILL;
                          } // switch (S2)
                      }
                      break;

                    default:
                      ILL;
                  } // switch (S1)
              }
              break;

            case 034: // ANA
              UNIMP;

            case 035: // ERA
              UNIMP;

            case 036: // SSA
              UNIMP;

            case 037: // ORA
              UNIMP;


// 40 - 47
            case 040: // ADCX3
              UNIMP;

            case 041: // LDX3
              UNIMP;

            case 042: // ADCX1
              UNIMP;

            case 043: // LDX1
              UNIMP;

            case 044: // LDI
              UNIMP;

            case 045: // TNC
              UNIMP;

            case 046: // ADQ
              UNIMP;

            case 047: // LDQ
              UNIMP;


// 50 - 57
            case 050: // STX3
              UNIMP;

            case 051: // ill
              ILL;

            case 052: // grp1c
              {
                switch (S1)
                  {
                    case 0:  // SIER
                      UNIMP;

                    case 4:  // SIC
                      UNIMP;

                    default:
                      ILL;
                  } // switch (S1)
              }
              break;

            case 053: // STX1
              UNIMP;

            case 054: // STI
              UNIMP;

            case 055: // TOV
              UNIMP;

            case 056: // STZ
              UNIMP;

            case 057: // STQ
              UNIMP;


// 60 - 67
            case 060: // CIOC
              UNIMP;

            case 061: // CMPX3
              UNIMP;

            case 062: // ERSA
              UNIMP;

            case 063: // CMPX1
              UNIMP;

            case 064: // TNZ
              UNIMP;

            case 065: // TPL
              UNIMP;

            case 066: // SBQ
              UNIMP;

            case 067: // CMPQ
              UNIMP;


// 70 - 77
            case 070: // STEX
              UNIMP;

            case 071: // TRA
              UNIMP;

            case 072: // ORSA
              UNIMP;

            case 073: // grp1a
              {
                switch (S1)
                  {
                    case 0:  // SEL
                      UNIMP;

                    case 1:  // IACX1
                      UNIMP;

                    case 2:  // IACX2
                      UNIMP;

                    case 3:  // IACX3
                      UNIMP;

                    case 4:  // ILQ
                      UNIMP;

                    case 5:  // IAQ
                      UNIMP;

                    case 6:  // ILA
                      UNIMP;

                    case 7:  // IAA
                      UNIMP;

                  } // switch (S1)
              }
              break;

            case 074: // TZE
              UNIMP;

            case 075: // TMI
              UNIMP;

            case 076: // AOS
              UNIMP;

            case 077: // ill
              ILL;

          }

        if (opcTable [OPCODE] . grp == opcMR)
          {
            if (opcTable [OPCODE] . opWR)
              {
                if (opcTable [OPCODE] . opSize == opW)
                  {
                    toMemory (& cpu, Y, W, C);
                  }
                else if (opcTable [OPCODE] . opSize == opDW)
                  {
                    toMemory36 (& cpu, YY, W, C);
                  }
              }
          }


// Instruction times vary from 1 us up.
        usleep (1);

      }
    while (reason == 0);
    return reason;
  }


