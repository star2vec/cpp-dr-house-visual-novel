#include "TerminalUI.h"

#include <chrono>
#include <iostream>
#include <thread>

#ifdef _WIN32
    #include <conio.h>
#else
    #include <termios.h>
    #include <unistd.h>
#endif

void TerminalUI::typewrite(const std::string& text, int delayMs) {
#ifdef _WIN32
    for (char c : text) {
        std::cout << c;
        std::cout.flush();
        int pause = (c == '.' || c == '!' || c == '?') ? 40 : delayMs;
        std::this_thread::sleep_for(std::chrono::milliseconds(pause));
    }
#else
    struct termios oldt, rawt;
    tcgetattr(STDIN_FILENO, &oldt);
    rawt = oldt;
    rawt.c_lflag &= ~(ICANON | ECHO);
    rawt.c_cc[VMIN] = 0;
    rawt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &rawt);

    bool skip = false;
    for (char c : text) {
        if (!skip) {
            char ch;
            if (read(STDIN_FILENO, &ch, 1) > 0) {
                skip = true;
                char drain[16];
                while (read(STDIN_FILENO, drain, sizeof(drain)) > 0) {}
            }
        }
        std::cout << c;
        if (!skip) {
            std::cout.flush();
            int pause = (c == '.' || c == '!' || c == '?') ? 40 : delayMs;
            std::this_thread::sleep_for(std::chrono::milliseconds(pause));
        }
    }
    std::cout.flush();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
}

void TerminalUI::clearScreen() {
    // ANSI escape code to clear screen
    std::cout << "\033[2J\033[1;1H";
}

int TerminalUI::getKeyPress() {
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
