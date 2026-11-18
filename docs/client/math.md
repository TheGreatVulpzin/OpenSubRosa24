# Vectors and random numbers

The client and server share `src/common/vectors.c` and `src/common/random.c`.

The vector operations write components in order. Overlapping inputs can therefore change later components. `tangentvector2d` writes negative y before it reads x. Matrix transposition reads all three rows before it writes the result.

`dotproduct2d` rounds its sum to a float. `vectorlength2d` keeps the square root in double precision. Quaternion conversion rounds nine products to floats before it forms the matrix entries.

The Mersenne Twister supplies signed integers and fractions with either endpoint included or excluded. `genrand_res53` combines two generator words. `getrandomint` returns sixteen low bits. `getrandomfloat` advances the separate hash counter once.

`fround` rounds to a multiple of a supplied step. A tie rounds toward positive infinity.
`net_calcdistancefactor` raises the distance to the power 1.75, stores its fraction as a float, then returns an integer weight.
`getenginetorque` clamps RPM to 0 through 8000 and applies two cubic curves.

`raydistancesphere2` returns the nearer distance along a unit ray. It stores the projection and perpendicular distance as floats before the square root. It stores the root as a float before the final subtraction.
`pointintersectcircle` writes a 2D direction and its distance. A point inside the circle gets a unit direction and a distance divided by the radius.
