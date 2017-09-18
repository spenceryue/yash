#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>


double GetTime() {
	struct timeval t;
	int rc = gettimeofday(&t, NULL);
	assert(rc == 0);
	return (double)t.tv_sec + (double)t.tv_usec/1e6;
}

void Spin(int howlong) {
	double t = GetTime();
	while ((GetTime() - t) < (double)howlong)
	; // do nothing in loop
}


int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, 0);

	char* str;
	if (argc == 1)
		str = "A";
	else
		str = argv[1];

	while (printf("%s ", str), Spin(1), 1);

	return 0;
}

