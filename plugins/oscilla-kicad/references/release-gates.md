# KiCad release gates

## Before fabrication export

1. The schematic matches the current Rev D pin/power authority.
2. ERC is clean, or every remaining finding has a documented and reviewed rationale.
3. PCB DRC is clean with schematic parity enabled.
4. A board render has been manually inspected for board outline, mounting, connector reachability, labeling, polarity, and Wio antenna clearance.
5. The production BOM and assembly orientation have been reviewed.

## Before ordering

1. Gerbers and drill data have been exported into a dated release directory and inspected in an independent viewer.
2. The fabrication stack-up, board thickness, copper weight, soldermask, finish, and assembly requirements have been selected deliberately.
3. The order notes identify rev A as requiring physical qualification: cold boot, USB recovery, 3V3 current/voltage behavior, receive soak, and enclosure fit.

Passing these gates does not prove RF behavior, regulator headroom, antenna performance, or correct assembly.
