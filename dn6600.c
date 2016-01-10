#include <unistd.h>
#include <stdbool.h>

#include "dn6600.h"
#include "dn6600_caf.h"
#include "coupler.h"
#include "iom.h"
#include "utils.h"

char sim_name [] = "dn6600";
int32 sim_emax = 1;
// DEVICE *sim_devices[] table of pointers to simulated devices, NULL terminated
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
    { "DEBUG",      DBG_DEBUG       },
    { "TRACE",      DBG_TRACE       },
    { "REG",        DBG_REG         },
    { "FINAL",      DBG_FINAL       },
    { "CAF",        DBG_CAF         },
    { NULL,         0               }
  };

static t_stat cpu_boot (UNUSED int32 unit_num, UNUSED DEVICE * dptr);

DEVICE cpuDev =
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
    NULL /*&cpu_dep*/,       /* deposit routine */
    NULL /*&cpu_reset*/,     /* reset routine */
    & cpu_boot,     /* boot routine */
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
    & couplerDev,
    NULL
  };

const char * sim_stop_messages [] =
  {
    "Stop"
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
    { "grp1d",   opcG1             }, // 12
    { "STX2",    opcMR,  OP_WR     }, // 13
    { "STAQ",    opcMR,  OP_DWR    }, // 14
    { "ADAQ",    opcMR,  OP_DRD    }, // 15
    { "ASA",     opcMR,  OP_RMW    }, // 16
    { "STA",     opcMR,  OP_WR     }, // 17

// 20 - 27
    { "SZN",     opcMR,  OP_RD     }, // 20
    { "DVF",     opcMR,  OP_RD     }, // 21
    { "grp1b",   opcG1             }, // 22
    { "CMPX2",   opcMR,  OP_RD     }, // 23
    { "SBAQ",    opcMR,  OP_DRD    }, // 24
    { "ill",     opcILL, OP_NULL   }, // 25
    { "SBA",     opcMR,  OP_RD     }, // 26
    { "CMPA",    opcMR,  OP_RD     }, // 27

// 30 - 37
    { "LDEX",    opcMR,  OP_NULL   }, // 30
    { "CANA",    opcMR,  OP_RD     }, // 31
    { "ANSA",    opcMR,  OP_RMW    }, // 32
    { "grp2",    opcG2             }, // 32
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
    { "grp1c",   opcG1             }, // 52
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
    { "grp1a",   opcG1             }, // 73
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

static void listSource (int offset)
  {
    FILE * lf = fopen ("gicb.list", "r");
    if (! lf)
      return;
    rewind (lf);
    char buffer [132];
    while (fgets (buffer, 132, lf))
      {
        int os;
        if (strncmp (buffer, "       ", 7) != 0)
          continue;
        if (strspn (buffer + 7, "01234567") != 5)
          continue;
        sscanf (buffer, " %o", & os);
        if (os == offset)
          {
            sim_debug (DBG_TRACE, & cpuDev, "%s", buffer);
            //break;
          }
      }
    fclose (lf);
  }

void doFault (int f, const char * msg)
  {
    //fprintf(stderr, "fault %05o : %s\n", f, msg);
    sim_printf ("fault %05o : %s\n", f, msg);
    // more later
    //longjmp (jmpMain, JMP_REENTRY);
    longjmp (jmpMain, JMP_STOP);
  }

static void doUnimp (word6 opc) NO_RETURN;

static void doUnimp (word6 opc)
  {
    sim_printf ("unimplemented %02o\n", opc);
    // more later
    longjmp (jmpMain, JMP_STOP);
  }

jmp_buf jmpMain;

t_stat sim_instr (void)
  {
    static int reason = 0;

    int val = setjmp (jmpMain);
    switch (val)
      {
        case JMP_ENTRY:
        case JMP_REENTRY:
          reason = 0;
          break;
        case JMP_STOP:
          goto leave;
        default:
          sim_printf ("longjmp value of %d unhandled\n", val);
          goto leave;
      }

    do
      {
        if (sim_interval <= 0)
          {
            if ((reason = sim_process_event ()))
              break;
          }
        sim_interval --;

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
        sim_debug (DBG_TRACE, & cpuDev, "%05o:%06o %s\n", cpu . rIC, ins, disassemble (ins));
        listSource (cpu . rIC);

        cpu . OPCODE = getbits18 (ins, 3, 6);
        cpu . I = 0;
        cpu . T = 0;
        cpu . D = 0;
        cpu . S1 = 0;
        cpu . S2 = 0;
        cpu . K = 0;
        cpu . W = 0;
        cpu . C = 0;
        cpu . Y = 0;
        cpu . YY = 0;
        cpu . NEXT_IC = (cpu . rIC + 1) & BITS15;

        switch (opcTable [cpu . OPCODE] . grp)
          {
            case opcILL:
              {
                // XXX fault illegal opcode
                break;
              }

            case opcMR:
              {
                cpu . I = getbits18 (ins, 0, 1);
                cpu . T = getbits18 (ins, 1, 2);
                cpu . D = getbits18 (ins, 9, 9);
                bool ok = doCAF (& cpu, cpu . I, cpu . T, cpu . D, & cpu . W, & cpu . C);
                if (! ok)
                  {
                    sim_printf ("doCAF failed\n");
                    longjmp (jmpMain, JMP_STOP);
                  }
                if (opcTable [cpu . OPCODE] . opRD)
                  {
                    if (opcTable [cpu . OPCODE] . opSize == opW)
                      {
                        cpu . Y = fromMemory (& cpu, cpu . W, cpu . C);
                      }
                    else if (opcTable [cpu . OPCODE] . opSize == opDW)
                      {
                        //cpu . YY = fromMemory36 (& cpu, cpu . W, cpu . C);
                        cpu . YY = fromMemory36 (& cpu, cpu . W, 1);
                      }
                  }
                break;
              }

            case opcG1:
              {
                cpu . S1 = getbits18 (ins, 0, 3);
                cpu . D = getbits18 (ins, 9, 9);
                sim_debug (DBG_DEBUG, & cpuDev, "grp1 S1 %o D %03o\n", cpu . S1, cpu . D);
                break;
              }
            case opcG2:
              {
                cpu . S1 = getbits18 (ins, 0, 3);
                cpu . S2 = getbits18 (ins, 9, 3);
                cpu . K = getbits18 (ins, 12, 6);
                break;
              }
          }

#define ILL doFault (faultIllegalOpcode, "illegal opcode")
#define UNIMP doUnimp (cpu . OPCODE);

        switch (cpu . OPCODE)
          {
            case 000: // illegal
              ILL;

            case 001: // MPF
              UNIMP;

            case 002: // ADCX2
              UNIMP;

            case 003: // LDX2
              // Load X2
              cpu . rX2 = cpu . Y;
              SCF (cpu . rX2 == 0, cpu . rIR, I_ZERO);
              break;

            case 004: // LDAQ
              // Load AQ
              cpu . rA = (cpu . YY >> 18) & BITS18;
              cpu . rQ = (cpu . YY >>  0) & BITS18;
              SCF (cpu . rA == 0 && cpu . rQ == 0, cpu . rIR, I_ZERO);
              SCF (getbits18 (cpu . rA, 0, 1) == 1, cpu . rIR, I_NEG);
              break;

            case 005: // ill
              ILL;

            case 006: // ADA
              UNIMP;

            case 007: // LDA
              // Load A
              cpu . rA = cpu . Y;
              SCF (cpu . rA == 0, cpu . rIR, I_ZERO);
              SCF (getbits18 (cpu . rA, 0, 1) == 1, cpu . rIR, I_NEG);
              break;


// 10 - 17
            case 010: // TSY
              // Transfer amd Store IC in Y
              
              cpu . Y = (cpu . rIC + 1) & BITS15;
              cpu . NEXT_IC = (cpu . W + 1) & BITS15;
              break;

            case 011: // ill
              ILL;

            case 012: // grp1d
              {
                switch (cpu . S1)
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
              // Store X2
              cpu . Y = cpu . rX2;
              break;

            case 014: // STAQ
              // Store AQ
              cpu . YY = (((word36) cpu . rA) << 18) | cpu . rQ;
              break;

            case 015: // ADAQ
              // Add to AQ
              {
                bool ovf;
                word36 tmp = ((word36) (cpu . rA) << 18) | cpu . rQ;
                sim_debug (DBG_TRACE, & cpuDev, "ADAQ     %012lo\n", tmp);
                sim_debug (DBG_TRACE, & cpuDev, "ADAQ +   %012lo\n", cpu . YY);
                word36 res = Add36b (tmp, cpu . YY, 0, I_ZERO | I_NEG | I_OVF | I_CARRY,
                                     & cpu . rIR, & ovf);
                sim_debug (DBG_TRACE, & cpuDev, "ADAQ =  %d%012lo\n", TSTF (cpu . rIR, I_CARRY) ? 1 : 0, res);
                //if (ovf and fault) XXX

                cpu . rA = (res >> 18) & BITS18;
                cpu . rQ = res & BITS18;
              }
              break;

            case 016: // ASA
              UNIMP;

            case 017: // STA
              // Store A
              cpu . Y = cpu . rA;
              break;


// 20 - 27
            case 020: // SZN
              UNIMP;

            case 021: // DVF
              UNIMP;

            case 022: // grp1b
              {
                switch (cpu . S1)
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
              // Subtract from AQ
              {
                bool ovf;
                word36 tmp = ((word36) (cpu . rA) << 18) | cpu . rQ;
                word36 res = Sub36b (tmp, cpu . YY, 1, I_ZERO | I_NEG | I_OVF | I_CARRY,
                                     & cpu . rIR, & ovf);
                //if (ovf and fault) XXX

                cpu . rA = (res >> 18) & BITS18;
                cpu . rQ = res & BITS18;
              }
              break;

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
                switch (cpu . S1)
                  {
                    case 0:
                      {
                        switch (cpu . S2)
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
                             // A Left Shift
                             {
                               // should a shift of 0 clear the carry?
                               CLRF (cpu . rIR, I_CARRY);
                               for (uint i = 0; i < cpu . K; i ++)
                                 {
                                   if (cpu . rA & BIT0)
                                     SETF (cpu . rIR, I_CARRY);
                                   cpu . rA = (cpu . rA << 1) & BITS18;
                                 }
                               SCF (cpu . rA == 0 && cpu . rQ == 0, cpu . rIR, I_ZERO);
                               SCF (getbits18 (cpu . rA, 0, 1) == 1, cpu . rIR, I_NEG);
                             }
                             break;

                            case 7: // ARS
                             UNIMP;

                            default:
                              ILL;
                          } // switch (S2)
                      }
                      break;

                    case 1:
                      {
                        switch (cpu . S2)
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
                        switch (cpu . S2)
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
                        switch (cpu . S2)
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

                            case 3: // CX3A 
                              // this may be a dd01 typo 2,33,3 fits the pattern -- no, looked
                              // at interpreter.list; cx3a is 3,33,3
                              // Copy X3 into A
                              cpu . rA = cpu . rX3;
                              break;

                            case 6: // ALP
                              // A Left Parity Rotate
                              // Rotate C(A) by Y (12-17) positions, enter each
                              // bit leaving position zero into position 17.
                              // Zero: If the number of 1's leavong position 0
                              // is even, then ON; otherwise OFF
                              // Negative: If (C(A)0 = 1, then ON; otherwise OFF
                              
                              {
                                int ones = 0;
                                for (uint n = 0; n < cpu . K; n ++)
                                  {
                                    word1 out = getbits18 (cpu . rA, 0, 1);
                                    cpu . rA <<= 1;
                                    cpu . rA |= out;
                                    if (out)
                                      ones ++;
                                  }
                                SCF (ones % 2 == 0, cpu . rIR, I_ZERO);
                                SCF (getbits18 (cpu . rA, 0, 1) == 1, cpu . rIR, I_NEG);
                              }
                              break;

                            default:
                              ILL;
                          } // switch (S2)
                      }
                      break;

                    case 4:
                      {
                        switch (cpu . S2)
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
                        switch (cpu . S2)
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
                        switch (cpu . S2)
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
                                for (uint n = 0; n < cpu . K; n ++)
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
              // OR to A
              cpu . rA |= cpu . Y;
              SCF (cpu . rA == 0, cpu . rIR, I_ZERO);
              SCF (getbits18 (cpu . rA, 0, 1) == 1, cpu . rIR, I_NEG);
              break;


// 40 - 47
            case 040: // ADCX3
              UNIMP;

            case 041: // LDX3
              // Load X3
              cpu . rX3 = cpu . Y;
              SCF (cpu . rX3 == 0, cpu . rIR, I_ZERO);
              break;

            case 042: // ADCX1
              UNIMP;

            case 043: // LDX1
              // Load X1
              cpu . rX1 = cpu . Y;
              SCF (cpu . rX1 == 0, cpu . rIR, I_ZERO);
              break;

            case 044: // LDI
              // Load I
              // C(Y) (Bits 0-7, 12-17) -> C(I)
              cpu . rIR = cpu . Y & 0776077;
              break;

            case 045: // TNC
              // Transfer on No Carry
              if (! TSTF (cpu . rIR, I_CARRY))
                {
                  cpu . NEXT_IC = cpu . W;
                }
              break;

            case 046: // ADQ
              UNIMP;

            case 047: // LDQ
              // Load Q
              cpu . rQ = cpu . Y;
              SCF (cpu . rQ == 0, cpu . rIR, I_ZERO);
              SCF (getbits18 (cpu . rQ, 0, 1) == 1, cpu . rIR, I_NEG);
              break;


// 50 - 57
            case 050: // STX3
              // Store X3
              cpu . Y = cpu . rX3;
              break;
              UNIMP;

            case 051: // ill
              ILL;

            case 052: // grp1c
              {
                switch (cpu . S1)
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
              // Store X1
              cpu . Y = cpu . rX1;
              break;

            case 054: // STI
              // Store I
              // C(I) (Bits 0-7, 12-17) -> C(Y)
              cpu . Y = cpu . rIR & 0776077;
              break;

            case 055: // TOV
              // Transfer on Overflow
              if (TSTF (cpu . rIR, I_OVF))
                {
                  cpu . NEXT_IC = cpu . W;
                  CLRF (cpu . rIR, I_OVF);
                }
              break;

            case 056: // STZ
              // Store Zero
              cpu . Y = 0;
              break;

            case 057: // STQ
              // Store Q
              cpu . Y = cpu . rQ;
              break;


// 60 - 67
            case 060: // CIOC
              // Connect Input/Output Channel

	    // "The CIOC instruction always accesses a double-precision
	    // (36-bit) Peripheral Control Word (PCW) and sends it, or
	    // portions thereof, to the channel indicated by the I/O channel
	    // Select Register. If the channel has a 6-, 9-, or 19-bit
	    // interface, it uses only part of the word."

              iomCIOC ();
              UNIMP;

            case 061: // CMPX3
              UNIMP;

            case 062: // ERSA
              UNIMP;

            case 063: // CMPX1
              UNIMP;

            case 064: // TNZ
              // Transfer on Not Zero
              if (! TSTF (cpu . rIR, I_ZERO))
                {
                  cpu . NEXT_IC = cpu . W;
                }
              break;

            case 065: // TPL
              // Transfer on Plus
              if (! TSTF (cpu . rIR, I_NEG))
                {
                  cpu . NEXT_IC = cpu . W;
                }
              break;

            case 066: // SBQ
              UNIMP;

            case 067: // CMPQ
              UNIMP;


// 70 - 77
            case 070: // STEX
              UNIMP;

            case 071: // TRA
              // Transfer unconditionally
              cpu . NEXT_IC = cpu . W;
              sim_debug (DBG_DEBUG, & cpuDev, "TRA %05o\n", cpu . NEXT_IC);
              break;

            case 072: // ORSA
              // OR to storage A
              cpu . Y |= cpu . rA;
              SCF (cpu . Y == 0, cpu . rIR, I_ZERO);
              SCF (getbits18 (cpu . Y, 0, 1) == 1, cpu . rIR, I_NEG);
              break;

            case 073: // grp1a
              {
                switch (cpu . S1)
                  {
                    case 0:  // SEL
                      UNIMP;

                    case 1:  // IACX1
                      // Immediate Add Character Adress to X1
                      // FA (C(X1), D] -> X1
                      {
                        int wx = SIGNEXT6 (cpu . D & BITS6);
                        word3 cx = (cpu . D >> 6) & BITS3;

                        // XXX C7 fix
                        if (cx == 07)
                          cx = 0;

                        int wy = SIGNEXT15 (cpu . rX1 & BITS15);
                        word3 cy = (cpu . rX1 >> 15) & BITS3;

                        word15 wz;
                        word3 cz;
                        addAddr32 (wx, cx, wy, cy, & wz, & cz);

                        cpu . rX1 = ((cz & BITS3) << 15) | (wz & BITS15);
                        SCF (cpu . rX1 == 0, cpu . rIR, I_ZERO);
                      }
                      break;

                    case 2:  // IACX2
                      // Immediate Add Character Adress to X2
                      // FA (C(X2), D] -> X2
                      {
                        int wx = SIGNEXT6 (cpu . D & BITS6);
                        word3 cx = (cpu . D >> 6) & BITS3;

                        // XXX C7 fix
                        if (cx == 07)
                          cx = 0;

                        int wy = SIGNEXT15 (cpu . rX2 & BITS15);
                        word3 cy = (cpu . rX2 >> 15) & BITS3;

                        word15 wz;
                        word3 cz;
                        addAddr32 (wx, cx, wy, cy, & wz, & cz);

                        cpu . rX2 = ((cz & BITS3) << 15) | (wz & BITS15);
                        SCF (cpu . rX2 == 0, cpu . rIR, I_ZERO);
                      }
                      break;

                    case 3:  // IACX3
                      // Immediate Add Character Adress to X3
                      // FA (C(X3), D] -> X3
                      {
                        int wx = SIGNEXT6 (cpu . D & BITS6);
                        word3 cx = (cpu . D >> 6) & BITS3;

                        // XXX C7 fix
                        if (cx == 07)
                          cx = 0;

                        int wy = SIGNEXT15 (cpu . rX3 & BITS15);
                        word3 cy = (cpu . rX3 >> 15) & BITS3;

                        word15 wz;
                        word3 cz;
                        addAddr32 (wx, cx, wy, cy, & wz, & cz);

                        cpu . rX3 = ((cz & BITS3) << 15) | (wz & BITS15);
                        SCF (cpu . rX3 == 0, cpu . rIR, I_ZERO);
                      }
                      break;

                    case 4:  // ILQ
                      // Immediate Load Q
                      cpu . rQ = SIGNEXT9 (cpu . D & 0777) & BITS18;
                      SCF (cpu . rQ == 0, cpu . rIR, I_ZERO);
                      SCF (getbits18 (cpu . rQ, 0, 1) == 1, cpu . rIR, I_NEG);
                      break;

                    case 5:  // IAQ
                      // Immediate Add Q
                      {
                        word18 tmp = SIGNEXT9 (cpu . D & 0777) & BITS18;
                        bool ovf;
                        cpu . rQ = Add18b (cpu . rQ, tmp, 0, I_ZERO | I_NEG | I_OVF | I_CARRY,
                                           & cpu . rIR, & ovf);
                        //if (ovf and fault) XXX
                      }
                      break;

                    case 6:  // ILA
                      // Immediate Load A
                      cpu . rA = SIGNEXT9 (cpu . D & 0777) & BITS18;
                      SCF (cpu . rA == 0, cpu . rIR, I_ZERO);
                      SCF (getbits18 (cpu . rA, 0, 1) == 1, cpu . rIR, I_NEG);
                      break;

                    case 7:  // IAA
                      // Immediate Add A
                      {
                        word18 tmp = SIGNEXT9 (cpu . D & 0777) & BITS18;
                        bool ovf;
                        cpu . rA = Add18b (cpu . rA, tmp, 0, I_ZERO | I_NEG | I_OVF | I_CARRY,
                                           & cpu . rIR, & ovf);
                        //if (ovf and fault) XXX
                      }
                      break;

                  } // switch (S1)
              }
              break;

            case 074: // TZE
              // Transfer on Zero
              if (TSTF (cpu . rIR, I_ZERO))
                {
                  cpu . NEXT_IC = cpu . W;
                }
              break;

            case 075: // TMI
              // Transfer on Minus
              if (TSTF (cpu . rIR, I_NEG))
                {
                  cpu . NEXT_IC = cpu . W;
                }
              break;

            case 076: // AOS
              // Add One to Storage
              cpu . Y = (cpu . Y + 1) & BITS18;
              SCF (cpu . Y == 0, cpu . rIR, I_ZERO);
              SCF (getbits18 (cpu . Y, 0, 1) == 1, cpu . rIR, I_NEG);
              break;

            case 077: // ill
              ILL;

          }

        if (opcTable [cpu . OPCODE] . grp == opcMR)
          {
            if (opcTable [cpu . OPCODE] . opWR)
              {
                if (opcTable [cpu . OPCODE] . opSize == opW)
                  {
                    toMemory (& cpu, cpu . Y, cpu . W, cpu . C);
                  }
                else if (opcTable [cpu . OPCODE] . opSize == opDW)
                  {
                    //toMemory36 (& cpu, cpu . YY, cpu . W, cpu . C);
                    toMemory36 (& cpu, cpu . YY, cpu . W, 1);
                  }
              }
          }

        sim_debug (DBG_REG, & cpuDev, "A: %06o Q: %06o X: %06o %06o %06o IR: %06o %s %s %s %s\n",
                   cpu . rA,
                   cpu . rQ,
                   cpu . rX1,
                   cpu . rX2,
                   cpu . rX3,
                   cpu . rIR,
                   TSTF (cpu . rIR, I_ZERO) ?  "Z" : "!Z",
                   TSTF (cpu . rIR, I_NEG) ?   "N" : "!N",
                   TSTF (cpu . rIR, I_CARRY) ? "C" : "!C",
                   TSTF (cpu . rIR, I_OVF) ?   "O" : "!O");

        cpu . rIC = cpu . NEXT_IC;

// Instruction times vary from 1 us up.
        usleep (1);

      }
    while (reason == 0);

leave:
    sim_printf("\nsimCycles = %0.0lf\n", sim_gtime ());

    return reason;
  }


// Temp boot code for testing
static t_stat cpu_boot (UNUSED int32 unit_num, UNUSED DEVICE * dptr)
  {
#define TALLY  00517

static uint64_t gicb [TALLY] =
  {
    0100002060002,
    0000000000070,
    0100000000000,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000722000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000722000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000722000736,
    0001000000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000722000736,
    0001005000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000722000736,
    0001012000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000722000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000722000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000722000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000722000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000722000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000722000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000722000736,
    0001017000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000722000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000722000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000722000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000736000736,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000725000725,
    0000725000725,
    0000725000725,
    0000725000725,
    0000000000000,
    0000000000000,
    0000000000000,
    0100454030002,
    0004260043202,
    0071112000000,
    0047771064046,
    0007305026303,
    0433200044161,
    0054760071005,
    0000000000000,
    0000000000000,
    0003166004277,
    0173776041002,
    0071057000505,
    0024266064026,
    0007263006154,
    0033200017167,
    0043170107000,
    0217000173001,
    0273001076164,
    0064773007244,
    0233703033614,
    0037240433300,
    0003242043134,
    0471150007126,
    0071006007123,
    0473000071003,
    0037122056106,
    0076105075715,
    0014112007221,
    0773006017063,
    0007217233703,
    0033606072064,
    0043072041002,
    0010033000630,
    0004050043103,
    0071012044667,
    0045002015063,
    0215000054663,
    0273002173776,
    0064771371001,
    0014656007072,
    0473070014655,
    0453053673002,
    0433200041002,
    0010005000454,
    0060646433100,
    0071777000000,
    0347000307001,
    0733622064002,
    0037036333622,
    0064002037034,
    0317001373002,
    0173776064765,
    0076763471762,
    0100632000006,
    0000000000075,
    0100656000001,
    0000000000073,
    0100000000000,
    0000000000070,
    0100000000000,
    0000016777775,
    0000000000001,
    0024000000000,
    0040000020000,
    0400000000000,
    0410000420000,
    0430000000463,
    0000733001000,
    0001002100452,
    0147000257000,
    0173001273001,
    0063006064773,
    0007005003766,
    0043767171777,
    0000000000000,
    0000670777764,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000433101,
    0071777000000,
    0433102071777,
    0433103071777,
    0433104071777,
    0433105071777,
    0000000471777,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000673003,
    0017232056775,
    0071020000000,
    0673004017225,
    0056775071013,
    0000000673005,
    0017220056775,
    0071006000000,
    0673014017213,
    0056775071001,
    0047431064703,
    0044624054423,
    0056706003113,
    0004136043115,
    0041002071526,
    0001036043112,
    0003105041002,
    0071521001043,
    0043106003101,
    0041002071514,
    0001050043102,
    0003075041002,
    0071507001055,
    0014054043051,
    0003051473740,
    0107000217000,
    0173001273001,
    0573001064773,
    0007144017612,
    0072560044557,
    0033604773002,
    0017555004033,
    0024676064631,
    0003042204016,
    0214776204014,
    0214000214002,
    0043056041002,
    0010477001154,
    0043053004037,
    0071456000000,
    0047016007016,
    0067644064415,
    0027643064413,
    0471001000460,
    0001174000740,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000000000000,
    0000002000420,
    0000456000460,
    0000376000030,
    0000002000552,
    0101156000004,
    0000000000074,
    0101134000004,
    0000000000070,
    0100000000000,
    0000012001117,
    0100002060002,
    0010101061002,
    0100742000010,
    0010600040076,
    0101000060000,
    0020600040076,
    0121000060000,
    0030600040076,
    0141000067204,
    0000000060070,
    0100000060000,
    0000000000070,
    0000000000000,
    0004500000037,
    0000000000000,
    0000000056410,
    0332302500004,
    0726346307451,
    0000000000000
  };

    for (int i = 0; i < TALLY; i ++)
      {
        uint64_t dw = gicb [i];
        uint32_t high = (dw >> 18) & BITS18;
        uint32_t low = dw & BITS18;
        cpu . M [i * 2] = high;
        cpu . M [i * 2 + 1] = low;
      }
    cpu . rIC = 512 + 1;
    //cpu . rIC = 0x100;
    return SCPE_OK;
  }
