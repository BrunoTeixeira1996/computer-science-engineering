#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#define MAX 256

int main(int argc, char* argv[]) {
	char result[MAX] = "Ol�! Este programa ainda n�o representa UNICODE";

	printf("Frase: %s\nTamanho: %d\n", result, strlen(result));

	getchar();

	return 0;
}