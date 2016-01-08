#include <stdbool.h>

#include "dn6600.h"

#define SIGN18 0400000
#define SIGN36 0400000000000

#define BIT19  01000000
#define BIT20  02000000
#define BIT37  01000000000000
#define BIT38  02000000000000

/*
 * 36-bit arithmetic stuff ...
 */
/* Single word integer routines */

word36 Add36b (word36 op1, word36 op2, word1 carryin, word18 flagsToSet, word18 * flags, bool * ovf)
  {

// https://en.wikipedia.org/wiki/Two%27s_complement#Addition
//
// In general, any two N-bit numbers may be added without overflow, by first
// sign-extending both of them to N + 1 bits, and then adding as above. The
// N + 1 bits result is large enough to represent any possible sum (N = 5 two's
// complement can represent values in the range −16 to 15) so overflow will
// never occur. It is then possible, if desired, to 'truncate' the result back
// to N bits while preserving the value if and only if the discarded bit is a
// proper sign extension of the retained result bits. This provides another
// method of detecting overflow—which is equivalent to the method of comparing
// the carry bits—but which may be easier to implement in some situations,
// because it does not require access to the internals of the addition.

    // 37 bit arithmetic for the above N+1 algorithm
    word38 op1e = op1 & BITS36;
    word38 op2e = op2 & BITS36;
    word38 ci = carryin ? 1 : 0;

    // extend sign bits
    if (op1e & SIGN36)
      op1e |= BIT37;
    if (op2e & SIGN36)
      op2e |= BIT37;

    // Do the math
    word38 res = op1e + op2e + ci;

    // Extract the overflow bits
    bool r37 = res & BIT37 ? true : false;
    bool r36 = res & SIGN36 ? true : false;

    // Extract the carry bit
    bool r38 = res & BIT38 ? true : false;

    // Check for overflow 
    * ovf = r37 ^ r36;

    // Check for carry 
    bool cry = r38;

    // Truncate the result
    res &= BITS36;
    if (flagsToSet & I_CARRY)
      {
        if (cry)
          SETF (* flags, I_CARRY);
        else
          CLRF (* flags, I_CARRY);
      }

    if (flagsToSet & I_OVF)
      {
        if (* ovf)
          SETF (* flags, I_OVF);      // overflow
      }

    if (flagsToSet & I_ZERO)
      {
        if (res)
          CLRF (* flags, I_ZERO);
        else
          SETF (* flags, I_ZERO);       // zero result
      }

    if (flagsToSet & I_NEG)
      {
        if (res & SIGN36)
          SETF (* flags, I_NEG);
        else
          CLRF (* flags, I_NEG);
      }

    return res;
  }

word36 Sub36b (word36 op1, word36 op2, word1 carryin, word18 flagsToSet, word18 * flags, bool * ovf)
  {

// https://en.wikipedia.org/wiki/Two%27s_complement
//
// As for addition, overflow in subtraction may be avoided (or detected after
// the operation) by first sign-extending both inputs by an extra bit.
//
// AL39:
//
//  If carry indicator ON, then C(A) - C(Y) -> C(A)
//  If carry indicator OFF, then C(A) - C(Y) - 1 -> C(A)

    // 38 bit arithmetic for the above N+1 algorithm
    word38 op1e = op1 & BITS36;
    word38 op2e = op2 & BITS36;
    // Note that carryin has an inverted sense for borrow
    word38 ci = carryin ? 0 : 1;

    // extend sign bits
    if (op1e & SIGN36)
      op1e |= BIT37;
    if (op2e & SIGN36)
      op2e |= BIT37;

    // Do the math
    word38 res = op1e - op2e - ci;

    // Extract the overflow bits
    bool r37 = (res & BIT37) ? true : false;
    bool r36 = (res & SIGN36) ? true : false;

    // Extract the carry bit
    bool r38 = res & BIT38 ? true : false;

    // Truncate the result
    res &= BITS36;

    // Check for overflow 
    * ovf = r37 ^ r36;

    // Check for carry 
    bool cry = r38;

    if (flagsToSet & I_CARRY)
      {
        if (cry) // Note inverted logic for subtraction
          CLRF (* flags, I_CARRY);
        else
          SETF (* flags, I_CARRY);
      }

    if (flagsToSet & I_OVF)
      {
        if (* ovf)
          SETF (* flags, I_OVF);      // overflow
      }

    if (flagsToSet & I_ZERO)
      {
        if (res)
          CLRF (* flags, I_ZERO);
        else
          SETF (* flags, I_ZERO);       // zero result
      }

    if (flagsToSet & I_NEG)
      {
        if (res & SIGN36)
          SETF (* flags, I_NEG);
        else
          CLRF (* flags, I_NEG);
      }

    return res;
  }

word36 Add18b (word18 op1, word18 op2, word1 carryin, word18 flagsToSet, word18 * flags, bool * ovf)
  {

// https://en.wikipedia.org/wiki/Two%27s_complement#Addition
//
// In general, any two N-bit numbers may be added without overflow, by first
// sign-extending both of them to N + 1 bits, and then adding as above. The
// N + 1 bits result is large enough to represent any possible sum (N = 5 two's
// complement can represent values in the range −16 to 15) so overflow will
// never occur. It is then possible, if desired, to 'truncate' the result back
// to N bits while preserving the value if and only if the discarded bit is a
// proper sign extension of the retained result bits. This provides another
// method of detecting overflow—which is equivalent to the method of comparing
// the carry bits—but which may be easier to implement in some situations,
// because it does not require access to the internals of the addition.

    // 19 bit arithmetic for the above N+1 algorithm
    word20 op1e = op1 & BITS18;
    word20 op2e = op2 & BITS18;
    word20 ci = carryin ? 1 : 0;

    // extend sign bits
    if (op1e & SIGN18)
      op1e |= BIT19;
    if (op2e & SIGN18)
      op2e |= BIT19;

    // Do the math
    word20 res = op1e + op2e + ci;

    // Extract the overflow bits
    bool r19 = (res & BIT19) ? true : false;
    bool r18 = (res & SIGN18) ? true : false;

    // Extract the carry bit
    bool r20 = res & BIT20 ? true : false;

    // Truncate the result
    res &= BITS18;

    // Check for overflow 
    * ovf = r19 ^ r18;

    // Check for carry 
    bool cry = r20;

    if (flagsToSet & I_CARRY)
      {
        if (cry)
          SETF (* flags, I_CARRY);
        else
          CLRF (* flags, I_CARRY);
      }

    if (flagsToSet & I_OVF)
      {
        if (* ovf)
          SETF (* flags, I_OVF);      // overflow
      }

    if (flagsToSet & I_ZERO)
      {
        if (res)
          CLRF (* flags, I_ZERO);
        else
          SETF (* flags, I_ZERO);       // zero result
      }

    if (flagsToSet & I_NEG)
      {
        if (res & SIGN18)
          SETF (* flags, I_NEG);
        else
          CLRF (* flags, I_NEG);
      }

    return res;
  }

word18 Sub18b (word18 op1, word18 op2, word1 carryin, word18 flagsToSet, word18 * flags, bool * ovf)
  {

// https://en.wikipedia.org/wiki/Two%27s_complement
//
// As for addition, overflow in subtraction may be avoided (or detected after
// the operation) by first sign-extending both inputs by an extra bit.
//
// AL39:
//
//  If carry indicator ON, then C(A) - C(Y) -> C(A)
//  If carry indicator OFF, then C(A) - C(Y) - 1 -> C(A)

    // 19 bit arithmetic for the above N+1 algorithm
    word20 op1e = op1 & BITS18;
    word20 op2e = op2 & BITS18;
    // Note that carryin has an inverted sense for borrow
    word20 ci = carryin ? 0 : 1;

    // extend sign bits
    if (op1e & SIGN18)
      op1e |= BIT19;
    if (op2e & SIGN18)
      op2e |= BIT19;

    // Do the math
    word20 res = op1e - op2e - ci;

    // Extract the overflow bits
    bool r19 = res & BIT19 ? true : false;
    bool r18 = res & SIGN18 ? true : false;

    // Extract the carry bit
    bool r20 = res & BIT20 ? true : false;

    // Truncate the result
    res &= BITS18;

    // Check for overflow 
    * ovf = r19 ^ r18;

    // Check for carry 
    bool cry = r20;

    if (flagsToSet & I_CARRY)
      {
        if (cry) // Note inverted logic for subtraction
          CLRF (* flags, I_CARRY);
        else
          SETF (* flags, I_CARRY);
      }

    if (flagsToSet & I_OVF)
      {
        if (* ovf)
          SETF (* flags, I_OVF);      // overflow
      }

    if (flagsToSet & I_ZERO)
      {
        if (res)
          CLRF (* flags, I_ZERO);
        else
          SETF (* flags, I_ZERO);       // zero result
      }

    if (flagsToSet & I_NEG)
      {
        if (res & SIGN18)
          SETF (* flags, I_NEG);
        else
          CLRF (* flags, I_NEG);
      }

    return res;
  }


