#include "tou.h"
#include "platform.h"
#include <stdarg.h>
#include <stdio.h>

int g_LogEnabled = 0;

/* Log Implementation (0046fb50) — only active when --logging is passed */
void Log(const char *format, ...) {
  if (!g_LogEnabled) return;

  char buffer[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  printf("%s", buffer);
  const std::string path = Platform_UserFilePath("debug.txt");
  FILE *f = path.empty() ? NULL : fopen(path.c_str(), "a");
  if (f) {
    fprintf(f, "%s", buffer);
    fclose(f);
  }
}
