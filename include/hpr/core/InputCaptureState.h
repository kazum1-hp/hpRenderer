#pragma once

// Supplied by the host/editor; InputManager does not query a UI library.
struct InputCaptureState
{
    bool mouseCaptured = false;
    bool keyboardCaptured = false;
    // True while hovered, or while an existing camera capture owns the viewport.
    bool viewportHovered = false;
};
