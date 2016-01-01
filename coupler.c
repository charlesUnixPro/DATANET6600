#include "global.h"
#include "coupler.h"

static t_stat couplerReset (DEVICE *dptr)
  {
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
    NULL,             /* attach routine */
    NULL,             /* detach routine */
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

