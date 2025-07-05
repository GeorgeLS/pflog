# Process Filter Logs

A easy to use utility that allows you to discard logs for specific processes.

## Overview

This code is a shared library that you can compile and use it with the `LD_PRELOAD/DYLD_INSERT_LIBRARIES` to quickly\
close stdout, stderr or both of specific processes based on the executable's name.
This is helpful when you have process trees (even small ones) and you want to focus on specific process output.

## Building the shared library

If you can, use the included `CMakeFiles.txt`. Otherwise, the compilation should be quite easy along the lines of:

```shell
clang -shared -O3 -o <pflog.so(linux)/pflog.dylib(macos)> pflog.c platform/[your_platform]/[your_platform].c
```

## Usage

Once you have the shared library compiled, you can use it via `LD_PRELOAD (linux)` or `DYLD_INSERT_LIBRARIES (MacOS)`.
Also, you must pass one of the following variables (or all of them depending on what you want to do):

- `PFLOG_FILTER`: This env variable contains the names of the executables you want to discard output for. Names are separated by colon (`:`). This env variable will close both stdout and stderr of the matched processes
- `PFLOG_STDOUT_FILTER`: The same as `PFLOG_FILTER` but instead discards only stdout
- `PFLOG_STDERR_FILTER`: The same as `PFLOG_FILTER` but instead discards only stderr

You can use any combination of those env variables to selectively discard stdout for some processes,\
stderr for some other and both for others.

Example usage:

```shell
LD_PRELOAD=<path/to/pflog.so> PFLOG_FILTER=ls ls
```

This should not produce any `ls` output.

Please note that in MacOS, there are restrictions for some core util binaries where Apple doesn't let you override\
behavior for those binaries using `DYLD_INSERT_LIBRARIES`. That is, if you use the example above, you will still see\
the output of `ls` command.
