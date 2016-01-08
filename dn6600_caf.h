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




word18 fromMemory  (cpu_t *cpu, word15 addr, int charAddr);
word36 fromMemory36(cpu_t *cpu, word15 addr, int charaddr);

void toMemory  (cpu_t *cpu, word18 data, word15 addr, word3 charaddr);
void toMemory36(cpu_t *cpu, word36 data, word15 addr, word3 charaddr);

bool doCAF(cpu_t *cpu, bool i, word2 t, word9 d, word15 *w, word3 *c);

word18 readITD  (cpu_t *cpu, bool i, word2 t, word9 d);
word36 readITD36(cpu_t *cpu, bool i, word2 t, word9 d);

void writeITD  (cpu_t *cpu, word18 data, bool i, word2 t, word9 d);
void writeITD36(cpu_t *cpu, word36 data, bool i, word2 t, word9 d);


#endif /* dn6600_caf_h */
