# C Parallel implementation of Prefix-sum 
A C and Pthreads implementation of the prefix sum algorithm.

The University of Edinburgh

**Originally Coursework** : Parallel Programming Languages and Systems

**Author** : Gavin Waite

Spring 2018

**Grade received**: A 

Confirmed to be working on macOS as of 2020-01-27

# Description

This project was the second coursework for the Parallel Programming Languages and Systems class in my 5th year at The University of Edinburgh. The original requirements and brief are found at `ex2.pdf`.

The task was to implement, using C and pthreads, a parallel implementation of the Prefix-sum algorithm. The challenge was to ensure timing and memory consistency between the parallel threads to speed up the output but still produce the correct result.

To run a test, the code will first compute the standard, trivial sequential prefix-sum before attempting the parallel method. It will validate the outputs are the same.

Code to run a test:
> gcc -o ex2 ex2.c -DNITEMS=10 -DNTHREADS=4 -DSHOWDATA=1 -lpthread

Where `NITEMS` controls the number of items in the prefix sum array and `NTHREADS` controls the number of threads used.

I included a test suite to test the system under various conditions up to 10,000,000 items and 32 threads. This can be run with:

> ./test.sh

