#pragma once

// Supplied by the host/editor; InputManager does not query a UI library.
struct InputCaptureState
{
    bool mouseCaptured = false;
    bool keyboardCaptured = false;
    bool viewportHovered = false;
};
