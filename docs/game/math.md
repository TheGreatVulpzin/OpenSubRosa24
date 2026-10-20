# Math

The math code is the base of all other systems. It has vector and orientation math, random numbers and intersection tests. Files: `src/common/vectors.c`, `random.c` and `intersect.c`.

## Records

| Record | Fields | Use |
| --- | --- | --- |
| `vec3` | `x`, `y`, `z` (float) | A position, a velocity or a direction. |
| `quat` | `x`, `y`, `z`, `w` (float) | A rotation. `w` is the scalar part. |
| orientation | `vec3 rot[3]` | A 3 by 3 rotation matrix. Each row is a `vec3`. |

The Mersenne Twister state is in two game globals: 624 words and an index. The index is 625 when no seed exists. The random vectors use a separate counter global.

## Rules for all functions

- The game computes in x87 registers. A store to memory rounds a value to a float. A value that stays in a register keeps double precision.
- The code has an explicit `(float)` cast at each point where the game rounds. A cast removed from the code changes the result.
- `sqrt`, `sin`, `cos` and `atan2` come from the x87 functions in `sr_math.h` (`x87_sqrt` and so on). A different libm gives a different last bit.
- The order of an addition is the order of the game. Floating point addition is not associative.
- Most functions read all inputs before the first write. The output can then be one of the inputs.

## Vectors

- `dotproduct` adds x and y first, then z.
- `normalizevector` returns a zero vector when the length is zero. It does not return NaN.
- `vectordistance` rounds each difference to a float, then computes the sum of squares from the unrounded differences.
- `projectvector` turns a vector by the three rows of an orientation.
- `generatetrinormal` is the unit normal of a triangle: (p1 - p0) x (p2 - p0).

## Orientations and angles

- `resetorientation` writes the identity matrix.
- `rotatevector` turns a vector around a unit axis (Rodrigues formula).
- `rotateorientation` turns each row around a unit axis. It then normalizes each row. Each step writes the row before the next step reads it.
- `multiplyquaternion` multiplies two orientations. The name comes from the game. The inputs are orientations, not quaternions.
- `multiply3x3matrices` starts each sum at 0. A product of -0 then gives +0.
- `geteulerangles2` gives the Euler angles of an orientation. Angle x comes from rows 0 and 1. Angle y comes from row 2. Angle z uses the sine and cosine of x.
- `orientationtoquaternion` uses the trace. A trace below -1 counts as -1. When the scale is below 1.52587890625E-5 or is NaN, the inverse is 0. The root and the inverse are doubles, not floats.
- `projectorientation` is the product of two quaternions.
- `angulardistance` gives a - b for two angles in radians. It moves one angle by a full turn when the difference is pi or more. The game uses `3.14159265359` for pi and `6.28318530718` for two pi.
- `normalizevector2d` and `rotatevector2d` do the same for x and y only.

## Random numbers

- `init_genrand` seeds MT19937. `genrand_int32` returns the next 32-bit word. When no seed exists, it seeds with 5489.
- The other random functions do not use the twister. They hash an integer. `hash_unit` gives a float in (-1, 1].
- `getrandomvector2d` and `getrandomvector3d` hash a global counter. The counter goes up by one for each value. The code makes a direction, normalizes it and sets the length between `min` and `min + range`.
- `serpnoise2d` is value noise. It hashes the four corners of a grid cell and mixes them with a smooth step (3t^2 - 2t^3). The `mask` argument wraps the cell numbers. It must be a power of two minus 1.
- The game code calls `os_rand()` for the C `rand` of the game. It never calls the CRT `rand`.

## Intersection tests

Each test returns 1 on a hit and 0 on a miss. A hit writes the hit point, the normal and the distance or the fraction. A line test uses `from` and `to`. The fraction `t` is the part of the line before the hit.

| Test | Rule |
| --- | --- |
| Line and plane | One sided: `from` must be on the back or on the plane. `to` must be on the front. A parallel line misses. |
| Line and triangle | Plane test, then three edge tests with the sign of each edge turn. |
| Line and sphere | `t` is the first hit. `n` is the unit vector from the center to the hit. |
| Sphere and line (an edge) | The nearest point of the edge must be between the ends and closer than the radius. |
| Sphere and triangle | The center must be in front of the plane and closer than the radius. The full version also tests the three edges. The nearest hit wins. |
| Circle and triangle | Used for wheels. The circle lies in the plane with the normal `rot`. The test finds the point of the triangle plane nearest to the center. Then it tests the three edges against the wheel plane. |
| 2D tests | A line, a circle and a segment in the x and y plane. |

Known odd behavior:

- `sphereintersectpoint` returns 0 also on a hit. The caller reads the output values, not the return value.
- The edge tests are written so that a NaN input gives a miss. The form of each compare matters.
- `circleintersecttriangle` sets `*dist` to the radius when there is no hit.
- `lineintersectline2d` hits only from the front side of the segment.

## Functions

| Function | RVA | File | Purpose |
| --- | --- | --- | --- |
| `dotproduct` | 0xb850 | vectors.c | Dot product of two vectors. |
| `crossproduct` | 0xb880 | vectors.c | Cross product. |
| `vectorlength` | 0xb8c0 | vectors.c | Length of a vector. |
| `normalizevector` | 0xb900 | vectors.c | Unit vector, or zero for a zero vector. |
| `addvectors` | 0xb9d0 | vectors.c | Sum of two vectors. |
| `subtractvectors` | 0xba00 | vectors.c | Difference of two vectors. |
| `scalevector` | 0xba30 | vectors.c | Vector times a scalar. |
| `scaleaddvectors` | 0xba60 | vectors.c | a + b * s. |
| `vectordistance` | 0xbaa0 | vectors.c | Distance between two points. |
| `projectvector` | 0xbb00 | vectors.c | Vector times the rows of an orientation. |
| `generatetrinormal` | 0xbb80 | vectors.c | Unit normal of a triangle. |
| `resetorientation` | 0xbc20 | vectors.c | Identity orientation. |
| `rotatevector` | 0xbc50 | vectors.c | Turn a vector around an axis. |
| `rotateorientation` | 0xbdb0 | vectors.c | Turn an orientation around an axis. |
| `normalizevector2d` | 0xbf60 | vectors.c | Unit vector in x and y. |
| `rotatevector2d` | 0xc0a0 | vectors.c | Turn x and y of a vector. |
| `angulardistance` | 0x1540 | vectors.c | Difference of two angles. |
| `geteulerangles2` | 0x9600 | vectors.c | Euler angles of an orientation. |
| `orientationtoquaternion` | 0xa7a0 | vectors.c | Quaternion of an orientation. |
| `projectorientation` | 0xa930 | vectors.c | Product of two quaternions. |
| `multiplyquaternion` | 0x1d8a0 | vectors.c | Product of two orientations. |
| `multiply3x3matrices` | 0x1d9e0 | vectors.c | Product of two 3 by 3 matrices. |
| `init_genrand` | 0xad30 | random.c | Seed the Mersenne Twister. |
| `genrand_int32` | 0xae50 | random.c | Next 32-bit random word. |
| `getrandomvector2d` | 0x21a30 | random.c | Random vector in x and y. |
| `getrandomvector3d` | 0x21b40 | random.c | Random 3D vector. |
| `serpnoise2d` | 0x9a00 | random.c | 2D value noise. |
| `lineintersectplane` | 0xc120 | intersect.c | Line against a plane, front side only. |
| `lineintersectplanetwosided` | 0xc290 | intersect.c | Line against a plane, both sides. |
| `lineintersecttriangle` | 0xc420 | intersect.c | Line against a triangle, front side only. |
| `lineintersecttriangletwosided` | 0xc680 | intersect.c | Line against a triangle, both sides. |
| `lineintersectsphere` | 0x1aa60 | intersect.c | Line against a sphere. |
| `calculatebarycentric` | 0x22af0 | intersect.c | Barycentric weights of a point on a triangle. |
| `sphereintersectline` | 0x1a7f0 | intersect.c | Sphere against an edge. |
| `sphereintersecttriangle` | 0x1a700 | intersect.c | Sphere against a triangle face. |
| `sphereintersecttrianglefull` | 0x307f0 | intersect.c | Sphere against a face and its three edges. |
| `circleintersecttriangle` | 0x1a000 | intersect.c | Wheel circle against a triangle. |
| `sphereintersectpoint` | 0x1a990 | intersect.c | Sphere against a point. |
| `pointdistanceline` | 0xc8f0 | intersect.c | Distance from a point to a segment. |
| `lineintersectline2d` | 0xcab0 | intersect.c | 2D line against a segment. |
| `sphereintersectline2d` | 0xcca0 | intersect.c | 2D circle against a segment. |
| `lineintersectcircle2d` | 0xcdf0 | intersect.c | 2D segment against a circle. |
