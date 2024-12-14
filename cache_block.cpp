#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <list>
#include <unordered_map>
#include <utility>

#define BLOCK_SIZE 16384
#define CACHE_SIZE 64


struct pair_hash {
    template <typename T1, typename T2>
    std::size_t operator() (const std::pair<T1, T2> &pair) const {
        auto hash1 = std::hash<T1>()(pair.first);
        auto hash2 = std::hash<T2>()(pair.second);
        return hash1 ^ (hash2 << 1); // Комбинируем хэши
    }
};


struct CacheBlock {
    int fd;
    off_t block_n;
    char *data;
    int modified;
    bool in_cache;
};


std::list<CacheBlock*> t1; // Используется для часто используемых блоков
std::list<CacheBlock*> t2; // Используется для редко используемых блоков

std::unordered_map<std::pair<int, off_t>, CacheBlock*, pair_hash> cache_map;
static int hit_count = 0;
static int miss_count = 0;

void init_cache() {
    cache_map.clear();
    t1.clear();
    t2.clear();
}

void free_cache() {
    for (auto& block : cache_map) {
        free(block.second->data);
        delete block.second;
    }
    cache_map.clear();
    t1.clear();
    t2.clear();
}

CacheBlock* find_block(int fd, off_t block_n) {
    auto it = cache_map.find(std::make_pair(fd, block_n));
    return (it != cache_map.end()) ? it->second : nullptr;
}

void move_to_t1(CacheBlock* block) {
    t1.push_front(block); 
    block->in_cache = true;
}

// Функция для вытеснения блоков в ARC
void evict_block() {
    if (!t1.empty()) {
        CacheBlock* block = t1.back();
        t1.pop_back();
        if (block->modified) {
            lseek(block->fd, block->block_n * BLOCK_SIZE, SEEK_SET);
            write(block->fd, block->data, BLOCK_SIZE);
        }
        cache_map.erase(std::make_pair(block->fd, block->block_n));
        free(block->data);
        delete block;
    } else if (!t2.empty()) {
        // Вытесняет блок из t2
        CacheBlock* block = t2.back();
        t2.pop_back();
        cache_map.erase(std::make_pair(block->fd, block->block_n));
        free(block->data);
        delete block;
    }
}

CacheBlock* load_block(int fd, off_t block_n) {
    CacheBlock* block = find_block(fd, block_n);
    if (block) {
        hit_count++;
        move_to_t1(block); // Переместить в t1, так как это "горячий" блок
        return block;
    }

    miss_count++;

    // Вытесняем блок, если кэш полон
    if (t1.size() + t2.size() >= CACHE_SIZE) {
        evict_block();
    }

    // Загружаем новый блок
    block = new CacheBlock();
    block->fd = fd;
    block->block_n = block_n;
    block->modified = 0;
    block->data = (char*)malloc(BLOCK_SIZE);
    
    if (block->data == nullptr) {
        perror("Ошибка выделения памяти для блока кэша");
        exit(EXIT_FAILURE);
    }

    lseek(fd, block_n * BLOCK_SIZE, SEEK_SET);
    read(fd, block->data, BLOCK_SIZE);
    
    cache_map[std::make_pair(fd, block_n)] = block;
    move_to_t1(block); // Добавляем в t1, так как это новый блок
    return block;
}

int lab2_open(const char *path) {
    int fd = open(path, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror("open error");
    }
    return fd;
}

int lab2_close(int fd) {
    // Сохранение всех измененных блоков, связанных с данным fd.
    for (auto it = cache_map.begin(); it != cache_map.end();) {
        if (it->second->fd == fd && it->second->modified) {
            lseek(fd, it->second->block_n * BLOCK_SIZE, SEEK_SET);
            write(fd, it->second->data, BLOCK_SIZE);
            it->second->modified = 0;
        }
        it++;
    }
    return close(fd);
}

ssize_t lab2_read(int fd, void *buf, size_t count) {
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

ssize_t lab2_write(int fd, const void *buf, size_t count) {
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

off_t lab2_lseek(int fd, off_t offset, int whence) {
    return lseek(fd, offset, whence);
}

int lab2_fsync(int fd) {
    // Сохраняя каждый блок, который модифицирован
    for (auto& block_pair : cache_map) {
        CacheBlock* block = block_pair.second;
        if (block->fd == fd && block->modified) {
            lseek(fd, block->block_n * BLOCK_SIZE, SEEK_SET);
            write(fd, block->data, BLOCK_SIZE);
            block->modified = 0;
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
