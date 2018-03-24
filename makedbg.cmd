python ../mbed.py compile -D MBED_STACK_STATS_ENABLED=1 --stats-depth=100 --profile=./debug.json -t GCC_ARM -m NUCLEO_F767ZI --build=./build/debug
echo * > .\build\debug\.mbedignore