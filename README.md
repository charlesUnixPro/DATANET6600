DATANET 6600 Front-End Network Processor (FNP).

Dox:

60132445 FEP Coupler Spec Nov77
DC88 DATANET 6600 FRONT-END NETWORK PROCESSOR
L66B Interrupt System 76122
DD01 DN35 assembler

gicb.map355

          rem       dia operation code symbol definitions
          rem
diatrg    bool      65        transfer gate from cs to fnp
diadis    bool      70        disconnect
diainf    bool      71        interrupt fnp
diajmp    bool      72        jump
diainc    bool      73        interrupt cs
diardc    bool      74        read configuration switches
diaftc    bool      75        data transfer from fnp to cs
diactf    bool      76        data transfer from cs to fnp
diawrp    bool      77        wraparound




DN355 (later marketed as DN6632) used with 6180

DC88, pg 12:
  DATANET 6600 DCP6632 [should be a relabeled DN355)

    32K memory
    DIA
    PSA interface to mass storage subsystem
    CCA console/printer
    TCU T&D cassette adaptor
    2(3) x General Purpose Communication Bases (CPCB)
               16 comm. chan. boards each.
    1(6) x Async. Comm. Bas Type 1 (ACB)
               24 slow lines

DN6661 (Reimplementation with new architecture)


AN85-01 pg 12-5:
load_fnp_: "The load_fnp_ subroutine prepares the core image for
bootload into the FNP. A segment is craeted in the process 
directory to contain the bootload program, bicb, and the core
image. This segment is laid out as follows:

     0 ----------------
       | boot dcw     |
     1 ----------------
       |              |
       |   gicb       |
       |              |
       |--------------|
       | padding to   |
       | mod 64       |
       |--------------|
       | core         |
       | image        |
       ----------------


AY34-02B DATANET Operation (DN6661)

+------------+     +---+                 +---+    +------------+
| Cache      |     |   |                 |   |    | Peripheral |
| Memory     |-----|   |                 |   |----| Interface  | ---- MPC ---- Disk Units
+------------+     |   |                 |   |    | Adpater    |
     |             |   |                 |   |    +------------+
+------------+     |   |                 |   |
| Central    |     |   |                 |   |      System       ---- Console
| Processor  |-----|   |                 |   |----  Support     
+------------+     |   |                 |   |      Controller   ---- Diskette
     |             |   |                 | I |
+------------+     |   |                 | N |
| Page       |     |   |                 | P |      Direct
| Unit       |-----| S |    +-------+    | U |----  Interface    ----  Host system
+------------+     | Y |    |       |    | T |      Adpater
                   | S |----|  IOM  |----| / |
+------------+     | T |    |       |    | O |
| 32K-Word   |     | E |    +-------+    | U |----  CIB/GPCB     ---- Up to 8 lines
| Memory     |-----| M |                 | T |
+------------+     |   |                 | P |
     .             | B |                 | U |----  Additional CIB/GPCB
     .             | U |                 | T |
     .             | S |                 |   |
     .             |   |                 | B |
+------------+     |   |                 | U |
| Additional |     |   |                 | S |
| 32K-Word   |-----|   |                 |   |
| Memory     |     |   |                 |   |
| Modules    |     |   |                 |   |
+------------+     +---+                 +---+



