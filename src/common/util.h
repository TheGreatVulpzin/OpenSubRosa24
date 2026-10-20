/* Small helpers of the game: the radix sort and the number text. See util.c. */
#ifndef SR_UTIL_H
#define SR_UTIL_H

/* Sorts the first Radix.count keys in ascending order (unsigned). Radix.order holds the item
 * numbers in key order, followed by -1. */
void csradixsort(void);

/* Writes a number with commas between the groups of three digits, for example 1,234,567. */
void sprintf_comma(char *out, int value);

#endif /* SR_UTIL_H */
