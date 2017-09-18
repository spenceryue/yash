#ifndef TOKENIZE_H
#define TOKENIZE_H

#include <stdio.h>			// FILE
#include <string.h>			// strtok, strlen

void replace_all (char* str, const char* key, const char replacement)
{
	while((str = strpbrk(str, key)) != NULL)
		*str = replacement;
}

#endif /* TOKENIZE_H */



/* Test TOKENIZE */
#if __INCLUDE_LEVEL__ == 0 && defined __INCLUDE_LEVEL__
#include <string.h>
#include <stdio.h>

int main(void)
{
	char string[]="Hi    there, Chip!";
	char *string_ptr = string;

	while((string_ptr=strpbrk(string_ptr,", "))!=NULL)
		*string_ptr='-';

	printf("New string is \"%s\".\n",string);

	replace_all(string, "-!", '0');
	replace_all(string, "~", '\0');

	printf("New string is \"%s\".\n",string);
	return 0;
}
#endif
/* Test TOKENIZE */