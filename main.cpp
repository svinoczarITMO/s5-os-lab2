#include <iostream>
#include <vector>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "cache_block.h"

const size_t BLOCK = 16 * 1024;

void read_block(int fd, std::vector<char>& block) {
    ssize_t bytes_read = lab2_read(fd, block.data(), block.size());
    if (bytes_read == -1) {
        std::cerr << "Ошибка чтения блока\n";
    }
}

void write_block(int fd, const std::vector<char>& block) {
    auto start = std::chrono::high_resolution_clock::now();
    ssize_t written = lab2_write(fd, block.data(), block.size());
    if (written == -1) {
        std::cerr << "Ошибка записи блока\n";
        return;
    }
    lab2_fsync(fd);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> result = end - start;
    std::cout << "time: " << result.count() << " sec" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Неверное количество аргументов. Использование: ./program <path> <n>\n";
        return 1;
    }

    std::string path = argv[1];
    int n = std::stoi(argv[2]);

    int fd_read = lab2_open(path.c_str());
    if (fd_read == -1) {
        std::cerr << "Ошибка открытия файла для чтения\n";
        return 1;
    }

    int fd_write = lab2_open("example_output.txt");
    if (fd_write == -1) {
        std::cerr << "Ошибка открытия файла для записи\n";
        lab2_close(fd_read);
        return 1;
    }

    std::vector<char> block(BLOCK, 0);

    for (int i = 0; i < n; ++i) {
        read_block(fd_read, block);
        write_block(fd_write, block);
    }

    if (lab2_close(fd_read) == -1) {
        std::cerr << "Ошибка закрытия файла для чтения\n";
        return 1;
    }

    if (lab2_close(fd_write) == -1) {
        std::cerr << "Ошибка закрытия файла для записи\n";
        return 1;
    }

    return 0;
}
