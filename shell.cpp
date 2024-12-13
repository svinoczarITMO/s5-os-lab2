#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <cstring>
#include <ctime>
#include "cache_block.h"

void trim(std::string &s) {
    s.erase(0, s.find_first_not_of(" \t"));
    s.erase(s.find_last_not_of(" \t") + 1);
}

int main() {
    std::string input;

    while (true) {
        std::cout << "$ ";
        std::getline(std::cin, input);

        if (input == "exit") {
            break;
        }

        std::vector<char*> args;
        char* token = std::strtok(const_cast<char*>(input.c_str()), " ");
        while (token != nullptr) {
            args.push_back(token);
            token = std::strtok(nullptr, " ");
        }
        args.push_back(nullptr);

        if (args[0] != nullptr && strcmp(args[0], "cd") == 0) {
            if (args[1] != nullptr) {
                if (chdir(args[1]) != 0) {
                    perror("cd");
                }
            } else {
                std::cerr << "cd: недостаточно аргументов" << std::endl;
            }
            continue;
        }

        pid_t pid = vfork();
        if (pid < 0) {
            perror("vfork");
            continue;
        }

        if (pid == 0) {
            execvp(args[0], args.data());
            perror("execvp");
            _exit(1);
        } else {
            clock_t start = clock();
            int status;
            waitpid(pid, &status, 0);
            clock_t end = clock();

            double elapsed_time = double(end - start) / CLOCKS_PER_SEC;
            std::cout << "Время выполнения: " << elapsed_time << " секунд" << std::endl;
        }
    }

    return 0;
}
