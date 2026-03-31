// naive implementation of range coding
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/*
struct CRangeDecoder
{
  uint32_t range;
  uint32_t code;
  uint8_t *stream; // check diff type
};
*/

/*
* (From lzma-specification)
* Each probability value must be initialized with value ((1 << 11) / 2),
* that represents the state, where probabilities of symbols 0 and 1 
* are equal to 0.5:
*/

// uint16_t Cprob = ((1 << 11) / 2);

int main()
{
  char string[] = "Hello world.\n"; // 14 character in total

  // simple range coder
  int code = 0;
  int low = 0;
  int range = 1;

  for (int i = 0; string[i] != '\0'; i++) {
    printf("%c",string[i]);
  }

  return 0;
}
