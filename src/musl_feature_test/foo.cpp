#include <string>
int foo() {
    volatile std::string v = std::to_string(1);
    return 0;
}
