#include <vector>
#include <sys/mman.h>
#include <iostream>
#include <cstring>
#include <unistd.h>

int main() {
    std::vector<char> v(100);
    if (mlock(v.data(), v.size()) == 0) {
        std::cout << "mlock succeeded" << std::endl;
        munlock(v.data(), v.size());
    } else {
        perror("mlock failed");
    }
    return 0;
}
