rm -R ./build
mkdir ./build
gcc src/*.c -o ./build/fused
./build/fused