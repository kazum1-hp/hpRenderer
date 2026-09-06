#pragma once
#include <sstream>
#include <string>

// Scoped capture: restore the original streams before destroying the buffer.
// Like the previous console, this is intended for the application's main thread.
class ConsoleCapture
{
public:
    ConsoleCapture();
    ~ConsoleCapture();
    ConsoleCapture(const ConsoleCapture&) = delete;
    ConsoleCapture& operator=(const ConsoleCapture&) = delete;
    std::string text() const { return buffer.str(); }
    void clear() { buffer.str(""); buffer.clear(); }
private:
    std::stringstream buffer;
    std::streambuf* previousOut;
    std::streambuf* previousError;
};
