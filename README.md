# asgn_1_bdsf22m034

# UNIX Shell Implementation
![Language](https://img.shields.io/badge/language-C-blue.svg)
![OS](https://img.shields.io/badge/platform-UNIX/Linux-lightgrey)
![Status](https://img.shields.io/badge/version-v6-success)
![License](https://img.shields.io/badge/license-MIT-green)

## Overview

This repository contains an implementation of a UNIX shell in C, developed as part of the Operating Systems Lab assignment. The shell acts as a command-line interpreter, allowing users to execute commands, handle input/output redirection, manage background processes, maintain command history, and use built-in commands.
Each version is implemented in a separate file (`shell1.c` to `shell6.c`). Compile with `gcc shellX.c -o shellX` and run using `./shellX` on Unix/Linux (e.g., Kali Linux).

---

## Code Status

- **Current Version:** All six versions of the shell have been implemented and tested.
- **Stability:** The shell is stable, but further testing is recommended, especially for edge cases.
- **Bugs Found:** 
  - **Background Processes:** Occasionally, background processes do not terminate as expected; further signal handling may be required.
  - **History Navigation:** The implementation of command history lacks support for up/down arrow key navigation. Currently, only `!number` is supported for command repetition.
  - **Input/Output Redirection:** Occasionally fails if files do not exist or if permission is denied, but this is generally handled with error messages.

---

## Features Implemented

### Version 01
- Displays a prompt (`PUCITshell:-`) and accepts user commands.
- Parses commands, forks new processes, and executes them.
- Parent process waits for the child process to terminate.
- Supports exiting the shell using `<CTRL+D>`.

### Version 02
- Input and output redirection using `<` and `>`.
- Supports piping commands (e.g., `command1 | command2`).

### Version 03
- Executes commands in the background using `&`.
- Implements signal handling to avoid zombie processes.

### Version 04
- Maintains a command history of the last 10 commands.
- Allows command repetition using `!number`.

### Version 05
- Implements built-in commands:
  - `cd`: Changes the working directory.
  - `exit`: Terminates the shell.
  - `jobs`: Lists background processes.
  - `kill`: Terminates a specified background process.
  - `help`: Displays available built-in commands and their syntax.

### Version 06 (BONUS)
- Adds support for user-defined and environment variables.
- Allows assignment, retrieval, and listing of variable values.

---

### Additional Features
- Implemented error handling for common issues like file not found and permission denied.
- Basic command syntax checking before execution.

---

## Acknowledgments

- **Richard Stevens' "Advanced Programming in the UNIX Environment"**: This book was instrumental in understanding UNIX system calls and shell implementation concepts.
- **Online Tutorials and Forums**: Various programming forums and resources (such as Stack Overflow) provided insights and solutions to specific coding challenges encountered during the project.
- **Peer Feedback**: Assistance and feedback from classmates helped improve the shell's functionality and user experience.

---

## 👩‍💻 Developer

**Amna Yaseen**  
[GitHub](https://github.com/amnaayaseen) • [LinkedIn](https://www.linkedin.com/in/amnaa-yaseen)

---

## 📄 License

This project is licensed under the **MIT License**.  
You are free to use, modify, and distribute it with proper credit.

© 2024 Amna Yaseen. All rights reserved.
