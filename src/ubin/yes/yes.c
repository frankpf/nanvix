#include <stddef.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
	char *str = argv[1];
	if (str == NULL) {
		str = "y";
	}

	while(1) {
		printf("%s\n", str);
	}

	return 0;
}
