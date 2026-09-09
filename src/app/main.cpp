#include "hpr/app/Application.h"
#include "hpr/core/RuntimePaths.h"
#include <iostream>

int main()
{
    try
    {
        SetExecutableWorkingDirectory();
        Application app("hpRenderer");
        app.run();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Startup failed: " << error.what() << '\n';
        return 1;
    }
}
