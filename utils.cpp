#include "tou.h"
#include <stdarg.h>
#include <stdio.h>


// Log Implementation (0046fb50)
void Log(const char *format, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  printf("%s", buffer);
  FILE *f = fopen("debug.txt", "a");
  if (f) {
    fprintf(f, "%s", buffer);
    fclose(f);
  }
}
