# My Custom Shell

This project implements a **POSIX-like shell** in C++ with features like command execution, pipelines, I/O redirection, history, and autocomplete using GNU Readline.

---

## Features

- **Command execution**: Run standard commands like `ls`, `pwd`, `echo`, `cat`, etc.
- **Change directory**: Supports `cd` with relative and absolute paths.
- **History**: Stores the last 20 commands in memory and in a history file.
- **Pipelines**: Supports commands connected with `|`.
- **I/O Redirection**: Supports `<`, `>`, `>>`.
- **Autocomplete**: Uses Readline to autocomplete commands and files.
- **Signal Handling**: Handles `Ctrl-C` (SIGINT) and `Ctrl-Z` (SIGTSTP) for foreground processes.

---

## Files

- `main.cpp`: Entry point of the shell. Handles prompt, user input, history, and command processing.  
- `shell.h`: Header file declaring functions, global variables, and signal handlers.  
- `shell.cpp`: Implements shell functionality like `cd`, `pwd`, `echo`, `ls`, `pipeline`, and autocomplete.  
- `Makefile`: Compiles the project with `g++` and links with Readline.

---

## Global Variables

- `string display`: Stores the shell prompt prefix (username@hostname).  
- `string host`: Hostname of the system.  
- `vector<string> directories`: Tracks directory history.  
- `vector<string> all`: Stores all commands.  
- `vector<string> cdfiles`: Stores all files/directories for `cd` autocompletion.  
- `vector<string> history`: Circular history buffer for last 20 commands.  
- `int history_ind`: Current index in the history buffer.  
- `int fg_pid`: Stores the PID of the foreground process.

---

## Functions Overview

- `vector<string> getcommands()` – Returns available commands.  
- `vector<string> getcdfiles()` – Returns directories/files for `cd`.  
- `string getDirectory()` – Returns current working directory for prompt.  
- `void maintainDirectory(char dir[])` – Updates directory history.  
- `void load_history_file()` / `void save_history_file()` – Manages persistent history.  
- `void cd(char* &rem)` – Implements `cd` command.  
- `void pwd(char* &rem)` – Prints current directory.  
- `void echo(char* &rem)` – Implements `echo`.  
- `void ls(char* &rem)` – Lists files.  
- `void pinfo(char* &rem)` – Shows process info.  
- `void search(char* &rem)` – Search utility.  
- `void pipeline(char* cmd)` – Handles commands with pipes.  
- `char* my_generator(const char* text, int state)` / `char** my_completion(const char* text, int start, int end)` – Readline autocomplete.  
- `void sigint_handler(int sig)` / `void sigtstp_handler(int sig)` – Signal handlers for Ctrl-C and Ctrl-Z.  

---

## Build Instructions

1. Ensure **g++** and **GNU Readline** library are installed:
   ```bash
   sudo apt install g++ libreadline-dev

2. Build the shell using the Makefile:
   ```bash
   make

3. Run the shell:
   ```bash
   ./my_shell

4. Clean object files and executable:
   ```bash
   make clean
