# Exercise 2: `forksort`

## Task
This assignment is here to implement an algorithm which sorts lines alphabetically using a recursive variant of merge sort

## Key concepts
This project focuses on process management and piping in C:
* The program splits the input lines into two parts
* It recursively executes the program in two child processes using `fork` and `execlp`
* It uses two unnamed pipes per child to redirect `stdin` and `stdout`
* The parent process merges the sorted parts from the two child processes
* At each step of the merge, it compares the next line of both parts and writes the smaller one to `stdout`
* The program uses `wait` or `waitpid` to read the exit status of the children