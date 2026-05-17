#ifndef TERMINAL_UI_H
#define TERMINAL_UI_H

#include <string>

// Static-only terminal I/O helpers. Definitions live in src/TerminalUI.cpp so the
// header doesn't drag <termios.h>/<conio.h>/<chrono>/<thread> into every TU that
// just wants to typewrite a string.
class TerminalUI {
public:
    static void typewrite(const std::string& text, int delayMs = 18);
    static void clearScreen();
    static int  getKeyPress();
};

#endif
