#include <stdio.h>
#include <stdint.h>

/*
* For each character, look backwards in the data stream to find matches. 
* 
* If there is a match, then encode the distance backwards, and the length 
* of the match instead of the raw bytes themselves. 
*
* If no match, encode the character itself as a literal. 
*
* Note that choosing matches is not a trivial process, and is the reason 
* why you pass different levels (amount of effort the compressor spends 
* searching for the best matches) into most compressors.
*/

int read_bit(uint32_t *bits, int *position)
{
  int r = *bits >> 31; // extract top bit
  *bits <<= 1;
  ++(*position);
  return r;
}

void refill(uint32_t *bits, uint8_t *current, uint8_t *end, int *position)
{
  while (position >= 0) {
    *bits |= (current < end ? *current : 0) << *position;
    *position -= 8;
    ++(*current);
  }
}

void flush(uint32_t *bits, uint8_t *current, int *position)
{
  *position -= 8; // we write out the eight bits that were written earliest
  *current = (*bits >> *position) & 0xFF;
  ++current;
}

void write_bit(int v, uint32_t *bits, int *position)
{
  *bits = (*bits << 1) | v;
  ++(*position);
  if (*position >= 8) {
    flush(bits,position);
  }
}

int main(int argc, char *argv[])
{
  if (argc != 2) {
    printf("Usage: %s <filename>", argv[0]);
  }

  uint32_t bits;
  uint8_t current;
  uint8_t end;
  int position;

  return 0;
}
