#include "TestSupport.h"

int main()
{
    stage14b03a::RunPositiveCases();
    stage14b03a::RunSimplificationCases();
    stage14b03a::RunFailureCases();
    std::cout << "Stage 14B-03A textured ViewData tests: PASS\n";
    return 0;
}
