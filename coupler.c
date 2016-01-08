#include "dn6600.h"
#include "coupler.h"
#include "udplib.h"

static t_stat couplerReset (DEVICE *dptr)
  {
    return SCPE_OK;
  }

static int32 link;

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
    ret = udp_create (cptr, & link);
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
    ret = udp_release (link);
    if (ret != SCPE_OK)
      return ret;
    link = NOLINK;
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

