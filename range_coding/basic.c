#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int low = 0;
int range = 100000;

void EmitDigit()
{
  printf("%i\n",low/10000);
  // Once we have a range of values with the same leading digit 
  // We can rescale 
	low = (low % 10000) * 10;
	range *= 10;
}
/*
          "Range"
 ____________________________
|                           |
       0.6   0.2     0.2
|A|A|A|A|A|A|B|B|<EOM>|<EOM>|
^
low
*/

void Encode(int start, int size, int total)
{
	// adjust the range based on the symbol interval
  // We start with a large range and shrink
	range /= total; // divide the range among unit segments 10 blocks distributed 6 for A, 2 for B etc
	low += start * range; // increase the low to point to start of block 
	range *= size; // Unit segments are scaled to size of block (A would be multiple of 6)

  // Leading digit must be the same throughout the range 
  // In while loop so we can emit multiple digits and shrink range 
  // by larger amount for symbols with high probablilities
	while (low / 10000 == (low + range) / 10000)
		EmitDigit();

	// readjust range - see reason for this below
	if (range < 1000)
	{
		EmitDigit();
		EmitDigit();
    // After two forced emits, low is always in [9xxxx, 1000000] so 1000000 - low 
    // is always in (0, 10000]
		range = 100000 - low;
	}
}

void Run()
{
  // probablilities A: 0.6, B: 0.2 <EOM>: 0.2
	Encode(0, 6, 10);	// A, low = 0, range (given prob) = 6, total range = 10
	Encode(0, 6, 10);	// A
	Encode(6, 2, 10);	// B
	Encode(0, 6, 10);	// A
	Encode(8, 2, 10);	// <EOM>

	// emit final digits - see below
	while (range < 10000)
		EmitDigit();

	low += 10000;
	EmitDigit();
}

int main(int argc, char *argv[])
{
  Run();
  return 0;
}
