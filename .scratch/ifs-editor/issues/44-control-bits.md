# Owning spans whose updates use different control bits

Status: ready-for-agent

Blocked by: 42.

Own refuses a span when one update carries flags the first update does not.
Over IIDX 33 that refuses 69174 spans (22076 root, 47098 sprite), the largest
group own cannot take. The flag bits that vary are the control bits: `0x4` use
matrix and `0x8` use colour, which the game needs set before it applies the
matrix or colour fields of a placement.

Measured over every placement in the install:

- A placement carrying a 2D matrix field always has `0x4`, and one carrying a
  colour field always has `0x8`. Never once the other way, in 24 million
  placements.
- The bits are not only derived: 375758 updates set `0x8` and 1563 set `0x4`
  with no such field, and every create sets `0x4` (122703 of them with no
  matrix field).

It also hides an editing bug: a colour keyed on a frame whose update only
carried a matrix is written without `0x8`, so the game ignores it.

## Acceptance

- The control bits written for a frame are the bits its written fields need,
  together with any the frame carried beyond those when it was owned, so an
  unedited span comes back exactly and an edited frame always carries the bit
  the game needs.
- Spans whose updates differ only in those two bits are owned.
- A placement carrying a field without the bit that applies it is refused,
  since it cannot come back exactly.
- Updates that differ in any other flag are still refused.
- Tested under `ci`, and the `local` own and detach survey still reports no
  differences while refusing fewer spans.
