// IOM Faults
//    0 -  7  MBZ
//    8 - 10  Data Command
//   11 - 13  Interrupt Command
//   14 - 17  Fault Type
//
//    Data Command
//      000 None
//      001 Load
//      010 Store
//      011 ADD
//      100 Subtract
//      101 AND
//      110 OR
//      111 Fault
//
//   Interrupt command
//      000 None
//      001 Uncondtional
//      010 Conditional or TRO (Tally Run Out)
//      011 Conditional or PTRO (Pre-Tally Run Out)
//      100 Conditional or Data Negative
//      101 Conditional or Zero
//      110 Conditional or Overflow
//      111 Fault
//
//   Fault Type
//     0000 None
//     0100 Program Fault
//     1000 Memory Parity Error
//     1100 Illegal Command to IOM
//     1010 Adder/Bus Parity
//     1001 Indirect Channel Detected Parity
//     1101 Direct Channel Detected Parity
//     1111 IOM Bus Priority Break

// The following code combinations cause an Illegal Channel Request Fault
//
//   Data  Interrupt
//    7     X
//    X     7
//    0     0
//    0     2
//    0     3
//    0     4
//    0     5
//    0     6


void iomCIOC (void);
