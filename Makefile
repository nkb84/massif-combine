all: massif-combine

massif-combine: src/massif-combine.cpp
	gcc -g -O2 -std=c++14 $< -o $@ -lstdc++
