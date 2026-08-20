#include <string.h>

int
strends(const char *A, const char *B)
{
	if(!A || !B)
		return 0;

	char *c = strrchr(A, B[0]);
	return c && strcmp(c, B) == 0 ? 1 : 0;
}
