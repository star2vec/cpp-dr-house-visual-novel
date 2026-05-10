#ifndef TERMINAL_UI_H
#define TERMINAL_UI_H

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
    #include <conio.h>
#else
    #include <termios.h>
    #include <unistd.h>
#endif

class TerminalUI {
public:
    static void typewrite(const std::string& text, int delayMs = 18) {
        for (char c : text) {
            std::cout << c;
            std::cout.flush();
            int pause = delayMs;
            if (c == '.' || c == '!' || c == '?') pause = 40;
            std::this_thread::sleep_for(std::chrono::milliseconds(pause));
        }
    }

    static void clearScreen() {
        // ANSI escape code to clear screen
        std::cout << "\033[2J\033[1;1H";
    }

    static int getKeyPress() {
#ifdef _WIN32
        int ch = _getch();
        if (ch == 224) {
            ch = _getch();
            if (ch == 72) return 1; // UP
            if (ch == 80) return 2; // DOWN
        }
        if (ch == 13) return 3; // ENTER
        return 0;
#else
        struct termios oldt, newt;
        int ch;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        ch = getchar();

        if (ch == 27) {
            getchar(); // skip '['
            int arrow = getchar();
            if (arrow == 65) ch = 1; // UP
            else if (arrow == 66) ch = 2; // DOWN
            else ch = 0;
        } else if (ch == 10) {
            ch = 3; // ENTER
        } else {
            ch = 0;
        }

        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return ch;
#endif
    }
};

#endif