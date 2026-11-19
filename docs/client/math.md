# Client math

The small math functions of the client are in `src/client/math/`.

## Vectors of two floats

`vectors2d.c` holds the set of functions that use the first two floats of a vector. The functions are `dotproduct2d`, `vectorlength2d`, `addvectors2d`, `subtractvectors2d`, `scalevector2d`, `scaleaddvectors2d` and `tangentvector2d`.

The length is the square root of the dot product. The code rounds the dot product to a float first.

`tangentvector2d` writes `(-y, x)`. It reads the second value after it writes the first. With the same vector for input and output, the result is `(-y, -y)`.

## Matrices and quaternions

`quaterniontoorientation` makes the 3 by 3 matrix of a unit quaternion (x, y, z, w). It doubles each product and rounds it to a float.

`transposematrix` copies the matrix before it writes. The input and the output can be the same matrix.

## Circles and engines

`pointintersectcircle` subtracts two 2D points and takes the length. It tells if the length is below the radius.

- When the length is below the radius, the vector becomes unit length.
- The length is then divided by the radius.
- The result uses the length from before the division.

`getenginetorque` cuts the speed to 0 to 8000 turns per minute and divides it by 8000. The value then goes through two bezier curves of degree 3. The first curve has the points 0, 0.125, 0.875 and 1. The second has 0.75, 1, 1 and 0.75.

## Packed unit vectors

`unpackunitvector` reads a unit vector from three words.

- Bits 0 and 1 of the first word name the axis. Bit 2 is the sign of that axis.
- The other two axes follow in the next two words. Bit 0 is the sign.
- The other bits are a value. The given number of bits scales it to the range 0 to 1.
- The result becomes unit length.
