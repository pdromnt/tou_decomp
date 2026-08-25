# Terrain Property Table

The original runtime creates a 256-entry table at `DAT_00487928`; each entry is
exactly `0x20` bytes. `FUN_0041EAE0` (`0041EAE0`) writes the defaults and every
hard-coded tile override before levels load.

`terrain_properties.h` deliberately types only that physical shape. It does
not expose `IsSolid`, `IsWalkable`, or `IsFluid`: individual consumers use the
same byte for narrower and sometimes different decisions, so a friendly global
name would currently claim more than the binary evidence proves.

## Observed consumers

| Offset | Current evidence |
| ---: | --- |
| `0x00` | Startup marks selected terrain classes active. |
| `0x01` | The original half-rate gameplay pass uses it while validating tile/entity interaction. |
| `0x02` | Startup groups several active and extended terrain families. |
| `0x03` | Set for the air/passable families and read by ship collision paths. |
| `0x04` | Set on overlay/entity terrain families. |
| `0x05` | Values 0, 1, or 2; ship movement reads it as a terrain response class. |
| `0x06`, `0x07` | Startup animation/display flags. |
| `0x08`, `0x09` | Replacement/cross-reference tile bytes used by terrain mutation. |
| `0x0A`, `0x0B` | Destruction and collision paths test these independently. |
| `0x0C`, `0x0D` | Rendering-layer decisions. |
| `0x0E` | Replacement tile selected by crushing/destruction code. |
| `0x12` | Default durability-like value; damage code consumes it. |
| `0x13..0x17` | Four animation members and the previous-link byte. |
| `0x18`, `0x19` | Ship terrain-response group and associated parameter. |
| `0x1A`, `0x1B` | Flags enabled for small special tile sets. |

These labels are evidence notes, not permission to coalesce checks. When a
caller is lifted or renamed, retain its exact byte offset, signedness, branch,
and original address until assembly/runtime comparison proves a stronger type.
