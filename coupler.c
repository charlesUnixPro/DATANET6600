#include <unistd.h>

#include "dn6600.h"
#include "coupler.h"
#include "udplib.h"

// dd01 pg 5-14 DATANET FNP Interface
//
// CIOC PCW: 100452000070
//
// PCW
//   0 -  2  001 Specifies and indirect 36 character address
//   3 - 16  Y
//  17       0
//  18 - 20  000
//  21       P1  Signifies the parity bit to make bits 0-17 odd parity.
//  22       P2  Signifies the parity bit to make bits 18-35 odd parity.
//  23       M   Specifies the DIA channel should be masked if this bit
//               is one. All other fields are ignored if the bit is one.
//  24 - 26 000
//  37 - 29 X
//  30 - 35 C
//
// C,X,Y:
//
//  C 73: Interrupt central system. X: Centeral Systrem IOM interrupt level 
//        no. to be set.
//  C: Any legal opcode except 73: Start DIA List Service Y: Address of LIST
//        ICW.
//

static struct
  {
    struct
      {
        word1 BT_INH; // Bootload Inhibit  60132445 pg 33
        word1 STORED_BOOT; // 60132445 pg 33
      } task_register;
    int32 link;
  } coupler_data;

static t_stat couplerReset (DEVICE *dptr)
  {
    coupler_data.task_register.BT_INH = 0;
    coupler_data.task_register.STORED_BOOT = 0;
    return SCPE_OK;
  }


static t_stat couplerAttach (UNIT * uptr, char * cptr)
  {
    //   simh calls this routine for (what else?) the ATTACH command.  There are
    // three distinct formats for ATTACH -
    //
    //    ATTACH -p MIn COMnn          - attach MIn to a physical COM port
    //    ATTACH MIn llll:w.x.y.z:rrrr - connect via UDP to a remote simh host
    //
    t_stat ret;
    char * pfn;
//uint16 line = uptr->mline;

    // If we're already attached, then detach ...
    if ((uptr -> flags & UNIT_ATT) != 0)
      detach_unit (uptr);

    // Make a copy of the "file name" argument.  udp_create() actually 
    // modifies the string buffer we give it, so we make a copy now so we'll 
    // have something to display in the "SHOW MIn ..." command.

    pfn = (char *) calloc (CBUFSIZE, sizeof (char));
    if (pfn == NULL)
      return SCPE_MEM;
    strncpy (pfn, cptr, CBUFSIZE);

    // Create the UDP connection.
    ret = udp_create (cptr, & coupler_data.link);
    if (ret != SCPE_OK)
      {
        free (pfn);
        return ret;
      };
    // Reset the flags and start polling ...
    uptr -> flags |= UNIT_ATT;
    uptr -> filename = pfn;
    //return couplerReset (find_dev_from_unit(uptr));
    return SCPE_OK;
  }

// Detach device ...
t_stat couplerDetach (UNIT * uptr)
  {
    // simh calls this routine for (you guessed it!) the DETACH command.  This
    // disconnects the modem from any UDP connection or COM port and effectively
    // makes the modem "off line".  A disconnected modem acts like a real modem
    // with its phone line unplugged.
    t_stat ret;
    if ((uptr -> flags & UNIT_ATT) == 0)
      return SCPE_OK;
    ret = udp_release (coupler_data.link);
    if (ret != SCPE_OK)
      return ret;
    coupler_data.link = NOLINK;
    uptr -> flags &= ~UNIT_ATT;
    free (uptr -> filename);
    uptr -> filename = NULL;
    //return mi_reset(PDEVICE(line));
    return SCPE_OK;
  }


static DEBTAB couplerDT [] =
  {
    { "DEBUG", DBG_DEBUG },
    { NULL, 0 }
  };

static UNIT couplerUnit =
  {
    UDATA (NULL, UNIT_FIX|UNIT_BINK, MEM_SIZE), 0, 0, 0, 0, 0, NULL, NULL
  };

static MTAB couplerMod [] =
  {
    { 0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
  };

DEVICE couplerDev = 
  {
    "COUPLER",        /* name */
    & couplerUnit,    /* units */
    NULL,             /* registers */
    couplerMod,       /* modifiers */
    1,                /* #units */
    8,                /* address radix */
    15,               /* address width */
    1,                /* address increment */
    8,                /* data radix */
    18,               /* data width */
    NULL,             /* examine routine */
    NULL,             /* deposit routine */
    couplerReset,     /* reset routine */
    NULL,             /* boot routine */
    couplerAttach,    /* attach routine */
    couplerDetach,    /* detach routine */
    NULL,             /* context */
    DEV_DEBUG,        /* flags */
    0,                /* debug control flags */
    couplerDT,        /* debug flag names */
    NULL,             /* memory size change */
    NULL,             /* logical name */
    NULL,             // attach help
    NULL,             // help
    NULL,             // help context
    NULL,             // device description
  };

#define psz 17000
static uint8_t pkt[psz];

// warning: returns ptr to static buffer
int poll_coupler (uint8_t * * pktp)
  {
    int sz = dn_udp_receive (coupler_data.link, pkt, psz);
    if (sz < 0)
      {
        sim_printf ("dn_udp_receive failed: %d\n", sz);
        sz = 0;
      }
    * pktp = pkt;
    return sz;
  }

void wait_for_boot (void)
  {
sim_printf ("waiting for boot signal\n");

// 60132445 pg 33

    if (coupler_data.task_register.BT_INH)
      {
        sim_printf ("WARNING: coupler waiting for boot with boot inhibit set\r\n");
      }

    uint8_t * pktp; 
    while (1)
      {
        int sz = poll_coupler (& pktp);
//sim_printf ("poll_coupler return %d\n", sz);
        if (! sz)
          {
            sleep (1);
            continue;
          }

#if 1
        sim_printf ("pkt[0] %hhu\r\n", pktp[0]);
        sim_printf ("sz: %d\n", sz);
        for (int i = 0; i < sz; i ++) sim_printf (" %03o", pktp[i]); sim_printf ("\r\n");
#endif
        if (pkt[0] != dn_cmd_bootload)
          {
            sim_printf ("got cmd %u; expected 1\r\n", pkt[0]);
            continue;
          }
        break;
      }

    // 60132445 pg 33
    coupler_data.task_register.STORED_BOOT = 1;

// "It obtains the L6 address and Tally from the TCW pointed to by the L66 
//  mailbox word and stores these internally; it also stores internaly, as
//  the L66 address for the Bootload, the address plus one of the above
//  Transfer Control Word"

    dn_bootload * p = (dn_bootload *) pktp;
//sim_printf ("dia_pcw: %12lo\r\n", p->dia_pcw);

    // The real coupler would at this point boot the DN CPU; it would run
    // it's boot PROM, and eventually:

    // pg 33: "The coupler then awaits the receipt of an Input Stored Boot
    // IOLD (FC=09) order (see 3.8).

    // pg 45:
    
    //  3.8 BOOTLOAD OF L6 BY L66
    //...
    // The receipt of the Input Stored Boot order causes the coupler,
    // if the Stored Boot but is ONE, to input data into L6 memory as
    // specified by the L66 Bootload order. On completion of this,
    // the Stored Boot bit is cleared.
    //
    // ... the PROM program issues the Input Stored Boot IOLD order
    // to the coupler..
    //
    // ... the L66 Bootload command specifies the L6 memory locations into
    // which the load from L66 is to occur and the extent of the lod;
    // location (0100)16 in L6 would always be the first location to be
    // executed by L6 after the load from L66 assuming that the L66
    // bootload is independent of the mechanization used in L66

    // Issued a Input Stored Boot IOLD order

    dn_ids_iold pkt;
    pkt.cmd = dn_cmd_ISB_IOLD;
    int rc = dn_udp_send (coupler_data.link, (uint8_t *) & pkt, sizeof (pkt), PFLG_FINAL);
    if (rc)
      {
        sim_printf ("dn_udp_send dn_cmd_ISB_IOLD returned %d\r\n", rc);
      }

// Wait for GICB

sim_printf ("waiting for buffer\n");
    while (1)
      {
        int sz = poll_coupler (& pktp);
//sim_printf ("poll_coupler return %d\n", sz);
        if (! sz)
          {
            sleep (1);
            continue;
          }

#if 1
        sim_printf ("pkt[0] %hhu\r\n", pktp[0]);
        sim_printf ("sz: %d\n", sz);
        for (int i = 0; i < sz; i ++) sim_printf (" %03o", pktp[i]); sim_printf ("\r\n");
#endif
        if (pktp[0] != dn_cmd_buffer)
          {
            sim_printf ("got cmd %u; expected 1\r\n", pktp[0]);
            continue;
          }
        break;
      }
  }
