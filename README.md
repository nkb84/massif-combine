# Massif combine

This is a tool to combine multiple massif output files into one.

## Motivation

- Memory profiling is an important in debugging or performane improvement
- [Valgrind Massif](https://valgrind.org/docs/manual/ms-manual.html) is a heap profiler, and not require much overhead like other profile. And it also provide a method to take the snapshot at specific time. Example: check heap right after a heavy task finish,...
- With that method, multiple massif output will be written, it's difficult to view/monitor the data accross multiple files
- This file will combine these files into one with the correct order

## Example to snapshot at a specificed place

```cpp
#include <valgrind/valgrind.h>

# At the place want to take snapshot
std::string command = "detailed_snapshot massif.vgdb." + std::to_string(time(NULL));
VALGRIND_MONITOR_COMMAND(command.c_str());
```

## How to build

- Install gcc
```shell
  make
```

## How to use

```
Usage: ./massif-combine [-o output] [-d] [-v] <file-pattern>...
                -o output: specify output file path
                -d: after combining, delete input files
                -v: verbose processing
                file-pattern: input file list, can include * character
                
Example:
       ./massif-combine -o massif.out.combine -d test/massif.out.13547 test/massif.vgdb.*
       massif-visualizer massif.out.combine
```

- Note: the program will use `ls <file pattern>`, so, please be careful.
