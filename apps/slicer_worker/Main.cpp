#include "WorkerApplication.h"

int main(int argc, char* argv[])
{
    const slicer_worker::WorkerApplication application;
    return application.Run(argc, argv);
}
