# Dynamic Memory Allocator

## For CSE 320 at Stony Brook University

## Description

The professor and team built an environment in which we were able to get memory pages from the OS through a custom function "sf_sbrk", and we had to implement malloc, free, and realloc functions, while maintaining a freelist of the free blocks. Specifically, we implemented an explicit freelist with immediate coallescing. You can see the functions I wrote in the 'src/sfmm.c' file. 