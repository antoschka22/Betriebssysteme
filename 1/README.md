# Exercise 1: `mygrep` and `3-Coloring`

## Task
This assignment is split into two parts: building a command-line text search utility and solving a graph problem using inter-process communication

## Part A: `mygrep`
`mygrep` is a reduced variation of the Unix-command grep
* The program reads files line by line and checks whether each line contains a specified keyword
* The line is printed if it contains the keyword
* It supports an `-i` option for case insensitive search
* It supports an `-o` option to write the output to a specified file

## Part B: `3-Coloring`
This implements an algorithm which makes a graph 3-colorable by removing the least edges possible
* The implementation uses a randomized algorithm
* Multiple `generator` processes generate random solutions and report them to one `supervisor` process
* The `supervisor` process remembers the best solution so far
* The processes communicate with each other by means of a circular buffer
* This buffer is implemented using shared semaphores and a shared memory