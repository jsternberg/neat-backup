
#include "util.h"
#include <stdio.h>

std::string ReadFile(const std::string& path) {
  FILE *fd = fopen(path.c_str(), "r");
  if (!fd) return std::string();

  std::string contents;
  char buf[4096];
  size_t sz;
  while ((sz = fread(buf, sizeof(char), 4096, fd)) > 0) {
    contents.append(buf, sz);
  }

  if (ferror(fd))
    return std::string();
  return contents;
}
