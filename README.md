# Command Shell
Coding a command shell capable of simple commands, background commands, conditional commands, redirections, pipes, and command interruptions.

## Description

This is a C++-based command shell capable of handling a variety of complex commands such as pipelines and interruptions. However, subshells and complex redirections are not supported. A linked list structure of storing commands is utilized (over a vector-based approach). The majority of the code is house in sh61.cc while essential helper functions are stored in helpers.cc.

## Getting Started

### Dependencies

* Linux OS

### Installing

* Download all files into a local directory

### Executing program

* Using the terminal, cd into that directory
* Run 
```
make sh61
```
* Run 
```
./sh61
```
* If successful, the file path will be replaced with 
```
sh61[[PROCESS_ID]]$
```

## Authors
* Bhushan Patel

## Acknowledgments
* COMPSCI: Systems Programming staff
