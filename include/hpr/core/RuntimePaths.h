#pragma once

// Existing runtime paths are relative to bin/ (or build/Debug and build/Release).
// Make launching from a terminal, Explorer, and an installed ZIP consistent.
void SetExecutableWorkingDirectory();
