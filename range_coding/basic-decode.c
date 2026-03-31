#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int code = 0;
int low = 0;
int range = 1;

int digits[] = {2,5,2,6};
int idx = 0;

void AppendDigit()
{
  if (idx < 3){
	  code = (code % 10000) * 10 + digits[idx];
  } else {
    code = (code % 10000) * 10;
  }
	low = (low % 10000) * 10;
	range *= 10;
}

void InitializeDecoder()
{
        AppendDigit();  // with this example code, only 1 of these is actually needed
        AppendDigit();
        AppendDigit();
        AppendDigit();
        AppendDigit();
}

void Decode(int start, int size, int total)  // Decode is same as Encode with EmitDigit replaced by AppendDigit
{
	// adjust the range based on the symbol interval
	range /= total;
	low += start * range;
	range *= size;

	// check if left-most digit is same throughout range
	while (low / 10000 == (low + range) / 10000)
		AppendDigit();

	// readjust range - see reason for this below
	if (range < 1000)
	{
		AppendDigit();
		AppendDigit();
		range = 100000 - low;
	}
}

int GetValue(int total)
{
	return (code - low) / (range / total);
}

void Run()
{
	int start = 0;
	int size;
	int total = 10;
	InitializeDecoder();          // need to get range/total >0
	while (start < 8)             // stop when receive EOM
	{
		int v = GetValue(total);  // code is in what symbol range?
		switch (v)                // convert value to symbol
		{
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5: start=0; size=6; printf("A"); break;
			case 6:
			case 7: start=6; size=2; printf("B"); break;
			default: start=8; size=2; break; 
		}
		Decode(start, size, total);
    if (start == 8) break;
	}
  printf("\n");
}

int main(int argc, char *argv[])
{
  Run();
  return 0;
}
