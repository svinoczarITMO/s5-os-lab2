#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <list>
#include <unordered_map>

#define BLOCK_SIZE 16384
#define CACHE_SIZE 64

typedef struct CacheBlock {
    int fd;
    off_t block_n;
    char *data;
    int modified;
} CacheBlock;

struct PairHash {
    std::size_t operator()(const std::pair<int, off_t>& p) const {
        std::size_t h1 = std::hash<int>()(p.first);
        std::size_t h2 = std::hash<off_t>()(p.second);
        return h1 ^ (h2 << 1);
    }
};

static std::list<CacheBlock*> cache_list;
static std::unordered_map<std::pair<int, off_t>, CacheBlock*, PairHash> cache_map;

void init_cache() {
    cache_list.clear();
}

void free_cache() {
    for (auto block : cache_list) {
        free(block->data);
        delete block;
    }
}

CacheBlock *find_block(int fd, off_t block_n) {
    auto it = cache_map.find(std::make_pair(fd, block_n));
    return it != cache_map.end() ? it->second : nullptr;
}

CacheBlock *evict_block() {
    if (cache_list.empty()) {
        return nullptr;
    }
    CacheBlock *block = cache_list.back();
    cache_list.pop_back();
    return block;
}

CacheBlock *load_block(int fd, off_t block_n) {
    CacheBlock *block = find_block(fd, block_n);
    if (block) return block;

    block = evict_block();
    if (!block) {
        block = new CacheBlock();
        block->data = (char*)malloc(BLOCK_SIZE);
        if (!block->data) {
            std::cerr << "Error allocating memory for cache block" << std::endl;
            exit(1);
        }
    }

    block->fd = fd;
    block->block_n = block_n;
    block->modified = 0;
    lseek(fd, block_n * BLOCK_SIZE, SEEK_SET);
    read(fd, block->data, BLOCK_SIZE);

    cache_map[std::make_pair(fd, block_n)] = block;
    cache_list.push_front(block);

    return block;
}


extern "C" int lab2_open(const char *path) {
    int fd = open(path, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror("open error");
        return -1;
    }

    // Отключение кэширования с помощью F_NOCACHE
    if (fcntl(fd, F_NOCACHE, 1) == -1) {
        perror("fcntl F_NOCACHE error");
        close(fd);
        return -1;
    }

    return fd;
}

extern "C" int lab2_close(int fd) {
    auto it = cache_map.begin();
    while (it != cache_map.end()) {
        if (it->second->fd == fd && it->second->modified) {
            lseek(fd, it->second->block_n * BLOCK_SIZE, SEEK_SET);
            write(fd, it->second->data, BLOCK_SIZE);
            it->second->modified = 0;
        }
        ++it;
    }
    return close(fd);
}

extern "C" ssize_t lab2_read(int fd, void *buf, size_t count) {
    char *buffer = (char *)buf;
    size_t bytes_read = 0;
    while (count > 0) {
        off_t block_n = lseek(fd, 0, SEEK_CUR) / BLOCK_SIZE;
        size_t block_offset = lseek(fd, 0, SEEK_CUR) % BLOCK_SIZE;
        CacheBlock *block = load_block(fd, block_n);
        size_t to_copy = (BLOCK_SIZE - block_offset) < count ? (BLOCK_SIZE - block_offset) : count;
        memcpy(buffer, block->data + block_offset, to_copy);
        buffer += to_copy;
        bytes_read += to_copy;
        count -= to_copy;
        lseek(fd, to_copy, SEEK_CUR);
    }
    return bytes_read;
}

extern "C" ssize_t lab2_write(int fd, const void *buf, size_t count) {
    const char *buffer = (const char *)buf;
    size_t bytes_written = 0;
    while (count > 0) {
        off_t block_n = lseek(fd, 0, SEEK_CUR) / BLOCK_SIZE;
        size_t block_offset = lseek(fd, 0, SEEK_CUR) % BLOCK_SIZE;

        CacheBlock *block = load_block(fd, block_n);
        block->modified = 1;

        size_t to_copy = (BLOCK_SIZE - block_offset) < count ? (BLOCK_SIZE - block_offset) : count;
        memcpy(block->data + block_offset, buffer, to_copy);

        buffer += to_copy;
        bytes_written += to_copy;
        count -= to_copy;

        lseek(fd, to_copy, SEEK_CUR);
    }
    return bytes_written;
}

extern "C" off_t lab2_lseek(int fd, off_t offset, int whence) {
    return lseek(fd, offset, whence);
}

extern "C" int lab2_fsync(int fd) {
    for (auto it = cache_map.begin(); it != cache_map.end(); ++it) {
        if (it->second->fd == fd && it->second->modified) {
            lseek(fd, it->second->block_n * BLOCK_SIZE, SEEK_SET);
            ssize_t bytes_written = write(fd, it->second->data, BLOCK_SIZE);
            if (bytes_written < 0) {
                perror("write error");
            }
            it->second->modified = 0;
        }
    }
    return 0;
}

__attribute__((constructor))
void cache_init() {
    init_cache();
}

__attribute__((destructor))
void cleanup_cache() {
    free_cache();
}
