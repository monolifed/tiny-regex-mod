#include <stdio.h>

//#define TRE_STATIC
#define TRE_IMPLEMENTATION
#include "tre.h"
//#undef TRE_STATIC
//#undef TRE_IMPLEMENTATION

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("Usage: %s pattern string\n", argv[0]);
		return 1;
	}
	tre_comp tregex;
	tre_compile(argv[1], &tregex);
	tre_print(&tregex);
	
	const char *string = argv[2];
	const char *end;
	const char *start = tre_match(&tregex, string, &end);
	
	if (start)
	{
		printf("match start: %zu match end: %zu\n", start - string, end - string);
	}
	else
	{
		printf("no match\n");
	}
	return 0;
}