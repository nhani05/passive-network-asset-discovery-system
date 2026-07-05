#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    std::cout << "PNAD backend service skeleton started.\n";
    std::cout << "Arguments:";
    for (int i = 1; i < argc; ++i) {
        std::cout << ' ' << argv[i];
    }
    std::cout << '\n';
    return 0;
}
