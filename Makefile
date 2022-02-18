all:
	clang count.c -o count -march=armv8-a+fp+simd+crypto+crc -O3 -lpthread
