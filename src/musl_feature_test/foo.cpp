#include <stdio.h>
#include <string>
int foo() { vsnprintf("a", 1, "x", nullptr); volatile std::string v = std::to_string(1); return 0; }
