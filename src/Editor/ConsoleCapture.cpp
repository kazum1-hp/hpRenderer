#include "Editor/ConsoleCapture.h"
#include <iostream>

ConsoleCapture::ConsoleCapture()
    : previousOut(std::cout.rdbuf(buffer.rdbuf())),
      previousError(std::cerr.rdbuf(buffer.rdbuf()))
{
}

ConsoleCapture::~ConsoleCapture()
{
    std::cout.rdbuf(previousOut);
    std::cerr.rdbuf(previousError);
}
