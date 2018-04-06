# PDP-coursework

This code is completely packaged and several files are included.

The c file "code.c" contains the main function and functions for suqirrel actors and land cell actors. 

The files like "squirrel-functions.c", "squirrel-functions.h","ran2.c" and "ran2.h" contain functions about squirrels' behaviors.

The "pool.c" and "pool.h" file include functions related to process pool.

Execution step:

1. Enter "make" in command line and the program will be compiled automatically. 

2. Enter "mpirun -np XX ./code" in command line.   

    XX means the number of processors you want to run for this program 
    
Execution on back-end nodes:

1. Enter "code.pbs" and code will be run on the back-end cirrus
