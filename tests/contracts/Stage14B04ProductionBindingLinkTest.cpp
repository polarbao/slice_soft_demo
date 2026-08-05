#include "slicer_core/engine/ProductionSliceFacadeFactory.h"

#include <iostream>
#include <memory>

int main()
{
    std::unique_ptr<slicer_core::api::SliceFacade> facade =
        slicer_core::engine::CreateProductionSliceFacade();
    if (!facade)
    {
        std::cerr << "Stage 14B-04 production binding link test: FAIL\n";
        return 1;
    }
    std::cout << "Stage 14B-04 production binding link test: PASS\n";
    return 0;
}
