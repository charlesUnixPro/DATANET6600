#include "dn6600.h"
#include "dn6600_caf.h"

void iomCIOC (void)
  {
    word36 PCW = fromMemory36 (& cpu, cpu . W, 1);
    sim_printf ("PCW %012lo S %o\n", PCW, cpu . rIR & 077);
  }

