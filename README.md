# Shell-OS-Project
Minishell_noexec - Implementation Summary
Project Overview
A Unix-like interactive command shell implemented in C without using any exec-family functions. External programs are executed using a custom ELF loader that loads and runs static binaries directly.

Core Features Implemented
1. Interactive Shell (REPL)
✅ Continuous loop waiting for user input
✅ Custom prompt displaying username@hostname:current_directory$
✅ Prompt updates correctly after directory changes
✅ Handles EOF (Ctrl+D) gracefully
✅ Non-interactive mode with -c flag for script execution
2. Command Parsing & Tokenization
✅ Whitespace handling (spaces, tabs, multiple spaces)
✅ Single quotes '...' (literal, no expansions)
✅ Double quotes "..." with escape sequences (\n, \", \\)
✅ Backslash escaping \ outside quotes
✅ Multiple commands separated by semicolons ;
✅ Pipeline support with |
✅ Redirection operators: <, >, >>
✅ Background execution with &
✅ Empty commands handling
3. Built-in Commands
File System Operations
✅ cd [dir] - Change directory (defaults to HOME if no argument)
✅ pwd - Print current working directory
✅ ls [path] - List directory contents
✅ touch file... - Create files or update timestamps
✅ mkdir [-p] dir... - Create directories (with -p for parent directories)
✅ rm [-rf] file/dir... - Remove files/directories (with -r for recursive, -f for force)
✅ cat [file...] - Display file contents (reads from stdin if no files)
Text Processing
✅ echo [-n] text... - Print text (with -n to suppress newline)
✅ grep pattern [file...] - Search for pattern in files or stdin
Environment & Variables
✅ export NAME=VALUE - Set environment variable
✅ unset NAME - Remove environment variable
Shell Management
✅ exit [code] - Exit shell with status code (default 0)
✅ jobs - List background jobs
✅ alias [name=value] - Create/display aliases
✅ unalias name - Remove alias
✅ history - Display command history
4. Pipelines (|)
✅ Arbitrary-length pipelines: cmd1 | cmd2 | cmd3
✅ Proper pipe file descriptor management
✅ Correct closing of unused pipe ends
✅ Works with builtins and external programs
✅ Combines with redirection
5. Redirection
✅ Input redirection: command < inputfile
✅ Output redirection (truncate): command > outfile
✅ Output redirection (append): command >> outfile
✅ Error handling for missing files and permission issues
✅ Combines with pipelines
6. Background Execution (&)
✅ Background job execution with trailing &
✅ Parent shell doesn't wait for background jobs
✅ Job tracking with PIDs
✅ Background job status reporting
✅ SIGCHLD handling for job reaping
7. External Program Execution (ELF Loader)
✅ Static ELF binary loader (no exec-family functions used)
✅ ELF header parsing and validation
✅ PT_LOAD segment mapping with mmap()
✅ Memory protection setup (PROT_READ, PROT_WRITE, PROT_EXEC)
✅ Stack preparation with argc, argv, envp
✅ Auxiliary vector setup
✅ Assembly trampoline for control transfer
✅ File descriptor handling for redirections
⚠️ Limitation: Only supports statically-linked ELF binaries (dynamic linking not implemented)
8. Process Groups & Job Control
✅ Process group management with setpgid()
✅ Terminal control with tcsetpgrp()
✅ Foreground job management
✅ Background job tracking
9. Signal Handling
✅ SIGINT (Ctrl-C) - Interrupts foreground job, doesn't exit shell
✅ SIGTSTP (Ctrl-Z) - Ignored by shell
✅ SIGCHLD - Handles background job completion
✅ Signal-safe I/O operations
10. Command History
✅ Stores up to 1000 commands in memory
✅ Persistent history saved to ~/.minishell_history
✅ Up/Down arrow keys for history navigation
✅ History browsing resets when typing new command
✅ history command to list all commands
11. Aliases
✅ alias name=value to create aliases
✅ alias to list all aliases
✅ unalias name to remove aliases
✅ Automatic alias expansion before command execution
✅ Recursive alias expansion (with depth limit to prevent infinite loops)
✅ Aliases work in interactive mode
12. Tab Completion
✅ Command completion - Completes builtin commands and PATH executables
✅ Filename completion - Completes files/directories from current directory
✅ Context-aware - Detects if completing command or filename
✅ Single match - Auto-completes if one match
✅ Multiple matches - Shows numbered list with descriptions
✅ Cycling - Press Tab again to cycle through matches
✅ User-friendly display - Serial numbers and command descriptions
13. Terminal Input Handling
✅ Raw terminal mode for advanced input
✅ Arrow key navigation:
Up/Down: History browsing
Left/Right: Cursor movement
✅ Backspace - Proper character deletion with line redraw
✅ Tab - Completion trigger
✅ Proper terminal state management
14. Error Handling & Robustness
✅ Comprehensive error checking for system calls
✅ Meaningful error messages
✅ Graceful handling of malformed input
✅ Memory leak prevention
✅ File descriptor leak prevention
✅ Shell doesn't crash on errors
Technical Implementation Details
Project Structure
src/
├── main.c          - Entry point, REPL loop
├── parser.c        - Command line parsing and tokenization
├── builtins.c      - All builtin command implementations
├── exec.c          - Command execution, pipelines, redirection
├── loader.c        - ELF binary loader (static only)
├── loader_trampoline.S - Assembly trampoline for ELF entry
├── jobs.c          - Background job management
├── signals.c       - Signal handling setup
├── history.c       - Command history management
├── aliases.c       - Alias management and expansion
├── completion.c    - Tab completion implementation
├── input.c         - Terminal input with history navigation
├── util.c          - Utility functions (xmalloc, prompt, etc.)
└── shell.h         - Shared headers and data structures
Key Data Structures
shell_state_t - Shell state (last status, running flag)
command_t - Parsed command structure (argv, redirs, pipes, background)
redir_t - Redirection linked list
token_t - Token for parsing
Build System
Makefile with proper dependency handling
Compiles to bin/minishell_noexec
Clean target for build artifacts
Known Limitations
ELF Loader: Only supports statically-linked binaries

Dynamic executables (most system binaries) cannot be run
Must compile test programs with gcc -static
Documented in code and should be in README
Aliases in -c mode: Aliases set and used in same command line may not work

Expansion happens before execution
Use interactive mode for full alias support
Advanced Features Not Implemented:

Here-documents (<<)
Command substitution (`cmd` or $(cmd))
Environment variable expansion in command lines ($VAR)
Advanced glob expansion (*, ?)
Full job control (fg/bg commands for specific jobs)
Testing Recommendations
Basic Functionality
✅ Builtin commands (cd, pwd, ls, echo, etc.)
✅ Pipelines (ls | grep src)
✅ Redirections (echo test > file.txt)
✅ Background jobs (sleep_static 2 &)
✅ History navigation (Up/Down arrows)
✅ Tab completion
✅ Aliases
Static Binary Execution
Create test programs: gcc -static -o test test.c
Run: ./test or test (if in PATH)
Summary Statistics
Total Builtin Commands: 15
Source Files: 13
Lines of Code: ~2000+ (estimated)
Features: All core requirements + history, aliases, completion
Compliance with Requirements
MUST Requirements ✅
✅ Interactive prompt
✅ Command parsing & tokenization (quotes, escapes, operators)
✅ Built-in commands (cd, pwd, exit, export, unset, jobs)
✅ External program execution without exec*() (static ELF loader)
✅ Pipes and pipelines
✅ Redirection (<, >, >>)
✅ Background execution (&)
✅ Signal handling (Ctrl-C)
✅ Error handling & robustness
SHOULD/NICE-TO-HAVE Features ✅
✅ Command history with arrow key navigation
✅ Aliases
✅ Tab completion for commands and filenames
✅ Additional builtins (ls, echo, grep, touch, mkdir, rm, cat)
