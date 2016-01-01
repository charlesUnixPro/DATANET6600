#include <unistd.h>

#include "global.h"
#include "coupler.h"

static word18 M [MEM_SIZE];



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

static word15 IC;
static word18 X [3];
static word18 A, Q;
static word18 I;
static word18 S;

static REG cpu_reg [] =
  {
    {ORDATA (IC, IC,    ASZ), 0, 0}, // Must be first, per simh
    {ORDATA (X1, X [0], WSZ), 0, 0},
    {ORDATA (X2, X [1], WSZ), 0, 0},
    {ORDATA (X3, X [1], WSZ), 0, 0},
    {ORDATA (A,  A,     WSZ), 0, 0},
    {ORDATA (Q,  Q,     WSZ), 0, 0},
    {ORDATA (I,  I,     8),   0, 0}, // Indicator register
    {ORDATA (S,  S,     6),   0, 0}, // I/O channel select register
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
//  | T=02  |  Y**=X2+D    |  Y*=C(X2+D)  |  Y**=X2+Y  |  Y*=C(X3+Y)  |
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
    enum { opcILL, opcMR, opcG1a, opcG1b, opcG1c, opcG1d, opcG2 } grp; // opcode group (memory, group1, group2)
  };

static struct opc_t opcTable [64] =
  {
// 00 - 07
    { "ill",     opcILL }, // 00
    { "MPF",     opcMR  }, // 01
    { "ADCX2",   opcMR  }, // 02
    { "LDX2",    opcMR  }, // 03
    { "LDAQ",    opcMR  }, // 04
    { "ill",     opcILL }, // 05
    { "ADA",     opcMR  }, // 06
    { "LDA",     opcMR  }, // 07

// 10 - 17
    { "TSY",     opcMR  }, // 10
    { "ill",     opcILL }, // 11
    { "grp1d",   opcG1d }, // 12
    { "STX2",    opcMR  }, // 13
    { "STAQ",    opcMR  }, // 14
    { "ADAQ",    opcMR  }, // 15
    { "ASA",     opcMR  }, // 16
    { "STA",     opcMR  }, // 17

// 20 - 27
    { "SZN",     opcMR  }, // 20
    { "DVF",     opcMR  }, // 21
    { "grp1b",   opcG1b }, // 22
    { "CMPX2",   opcMR  }, // 23
    { "SBAQ",    opcMR  }, // 24
    { "ill",     opcILL }, // 25
    { "SBA",     opcMR  }, // 26
    { "CMPA",    opcMR  }, // 27

// 30 - 37
    { "LDEX",    opcMR  }, // 30
    { "CANA",    opcMR  }, // 31
    { "ANSA",    opcMR  }, // 32
    { "grp2",    opcG2  }, // 32
    { "ANA",     opcMR  }, // 34
    { "ERA",     opcMR  }, // 35
    { "SSA",     opcMR  }, // 36
    { "ORA",     opcMR  }, // 37

// 40 - 47
    { "ADCX3",   opcMR  }, // 40
    { "LDX3",    opcMR  }, // 41
    { "ADCX1",   opcMR  }, // 42
    { "LDX1",    opcMR  }, // 43
    { "LDI",     opcMR  }, // 44
    { "TNC",     opcMR  }, // 45
    { "ADQ",     opcMR  }, // 46
    { "LDQ",     opcMR  }, // 47

// 50 - 57
    { "STX3",    opcMR  }, // 50
    { "ill",     opcILL }, // 51
    { "grp1c",   opcG1c }, // 52
    { "STX1",    opcMR  }, // 53
    { "STI",     opcMR  }, // 54
    { "TOV",     opcMR  }, // 55
    { "STZ",     opcMR  }, // 56
    { "STQ",     opcMR  }, // 57

// 60 - 67
    { "CIOC",    opcMR  }, // 60
    { "CMPX3",   opcMR  }, // 61
    { "ERSA",    opcMR  }, // 62
    { "CMPX1",   opcMR  }, // 63
    { "TNZ",     opcMR  }, // 64
    { "TPL",     opcMR  }, // 65
    { "SBQ",     opcMR  }, // 66
    { "CMPQ",    opcMR  }, // 67

// 70 - 77
    { "STEX",    opcMR  }, // 70
    { "TRA",     opcMR  }, // 71
    { "ORSA",    opcMR  }, // 72
    { "grp1a",   opcG1a }, // 73
    { "TZE",     opcMR  }, // 74
    { "TMI",     opcMR  }, // 75
    { "AOS",     opcMR  }, // 76
    { "ill",     opcILL } // 77

  };

static char * disassemble (word18 ins)
  {
    static char result[132] = "???";
    word6 OPCODE = getbits18 (ins, 3, 6);
    strcpy (result, opcTable [OPCODE] . name);
    return result;
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

        word18 ins = M [IC];
        sim_debug (DBG_TRACE, & cpuDev, "%05o %06o %s\n", IC, ins, disassemble (ins));

        word6 OPCODE = getbits18 (ins, 3, 6);
        word1 I = 0;
        word2 T = 0;
        word9 D = 0;
        word3 S1 = 0;
        word3 S2 = 0;
        word6 K = 0;
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
                // XXX do CAF; break out into R,W,RMW
                break;
              }
            case opcG1a:
            case opcG1b:
            case opcG1c:
            case opcG1d:
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

        switch (OPCODE)
          {
            case 000: // illegal
            case 001: // MPF
            case 002: // ADCX2
            case 003: // LDX2
            case 004: // LDAQ
            case 005: // ill
            case 006: // ADA
            case 007: // LDA

// 10 - 17
            case 010: // TSY
            case 011: // ill
            case 012: // grp1d
            case 013: // STX2
            case 014: // STAQ
            case 015: // ADAQ
            case 016: // ASA
            case 017: // STA

// 20 - 27
            case 020: // SZN
            case 021: // DVF
            case 022: // grp1b
            case 023: // CMPX2
            case 024: // SBAQ
            case 025: // ill
            case 026: // SBA
            case 027: // CMPA

// 30 - 37
            case 030: // LDEX
            case 031: // CANA
            case 032: // ANSA
            case 033: // grp2
            case 034: // ANA
            case 035: // ERA
            case 036: // SSA
            case 037: // ORA

// 40 - 47
            case 040: // ADCX3
            case 041: // LDX3
            case 042: // ADCX1
            case 043: // LDX1
            case 044: // LDI
            case 045: // TNC
            case 046: // ADQ
            case 047: // LDQ

// 50 - 57
            case 050: // STX3
            case 051: // ill
            case 052: // grp1c
            case 053: // STX1
            case 054: // STI
            case 055: // TOV
            case 056: // STZ
            case 057: // STQ

// 60 - 67
            case 060: // CIOC
            case 061: // CMPX3
            case 062: // ERSA
            case 063: // CMPX1
            case 064: // TNZ
            case 065: // TPL
            case 066: // SBQ
            case 067: // CMPQ

// 70 - 77
            case 070: // STEX
            case 071: // TRA
            case 072: // ORSA
            case 073: // grp1a
            case 074: // TZE
            case 075: // TMI
            case 076: // AOS
            case 077: // ill

            default:
              {
                //XXX
              }
          }

// Instruction times vary from 1 us up.
        usleep (1);

      }
    while (reason == 0);
    return reason;
  }


