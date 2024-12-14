g++ -fPIC -shared -std=c++11 -o out/libblock_cache.so cache_block.cpp
g++ -std=c++11 -o out/main main.cpp -L. -lblock_cache -ldl
g++ -std=c++11 -o out/shell shell.cpp -L. -lblock_cache -ldl
g++ -std=c++11 -o out/combined combined.cpp -L. -lblock_cache -ldl -lpthread