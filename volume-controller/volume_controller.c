#include <libserialport.h>
#include <stdio.h>

#define FILE_NAME "/dev/ttyUSB0"

int main() {

  FILE *fptr = fopen(FILE_NAME, "r");
  char buffer[1024] = {0};

  if (fptr == NULL) {
    printf("Could not open file %s", FILE_NAME);
    return 1;
  }

  while (fgets(buffer, sizeof(buffer), fptr) != NULL) {
    printf("%s", buffer);
  }

  fclose(fptr);
  return 0;
}
