# Vectors and random numbers

The client and server share `src/common/vectors.c` and `src/common/random.c`.

The vector operations write components in order. Overlapping inputs can therefore change later components. `tangentvector2d` writes negative y before it reads x. Matrix transposition reads all three rows before it writes the result.

`dotproduct2d` rounds its sum to a float. `vectorlength2d` keeps the square root in double precision. Quaternion conversion rounds nine products to floats before it forms the matrix entries.

The Mersenne Twister supplies signed integers and fractions with either endpoint included or excluded. `genrand_res53` combines two generator words. `getrandomint` returns sixteen low bits. `getrandomfloat` advances the separate hash counter once.
