g++ -std=c++11 -fPIC -shared -o libcache_block.so cache_block.cpp
g++ -std=c++11 -o out/main main.cpp -L. -lcache_block
g++ -std=c++11 -o out/shell shell.cpp -L. -lcache_block
g++ -std=c++11 -o out/combined combined.cpp -L. -lcache_block
export DYLD_LIBRARY_PATH=.