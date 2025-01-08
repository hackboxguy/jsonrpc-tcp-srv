#!/bin/sh
which clang-format > /dev/null
[ $? != 0 ] && echo "clang-format not found, use apt install clang-format to install" && exit 1
find lib/lib-jsonrpc-tcp/ -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c" | xargs clang-format -i
find lib/lib-display/ -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c" | xargs clang-format -i
find lib/lib-settings/ -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c" | xargs clang-format -i
find services/ -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c" | xargs clang-format -i
find utils/ -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c" | xargs clang-format -i
