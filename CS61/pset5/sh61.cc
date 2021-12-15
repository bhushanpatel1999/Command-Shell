#include "sh61.hh"
#include <cstring>
#include <cerrno>
#include <vector>
#include <sys/stat.h>
#include <sys/wait.h>
#include <iostream>
#include <fcntl.h>  
#include <unistd.h>

using namespace std;
// For the love of God
#undef exit
#define exit __DO_NOT_CALL_EXIT__READ_PROBLEM_SET_DESCRIPTION__

//variable for signal handler
volatile sig_atomic_t term_signal = 0;

// Set up signal handler
void handle_sig(int signal) {
    (void) signal;
    term_signal = 1;
}

// struct command
//    Data structure describing a command. Add your own stuff.
struct command {
    std::vector<std::string> args;
    // Pointers to next and previous nodes
    command* next = nullptr;
    command* prev = nullptr;
    pid_t pid = -1;      // process ID running this command, -1 if none

    // Stores exit status of child process
    int status;

    command();
    ~command();

    // Edit to take group pid as input
    pid_t run(pid_t groupid);

    // type of connection to next command in linked list
    int link; 

    // Name of redirection files
    std::string input_file;
    std::string output_file;
    std::string error_file;

    // Bool for cd command
    bool is_cd = false;
};


// command::command()
//    This constructor function initializes a `command` structure. You may
//    add stuff to it as you grow the command structure.

command::command() {
}


// command::~command()
//    This destructor function is called to delete a command.

command::~command() {
}


// Struct to store conditional chains
struct chain {
    command* start = nullptr;

    bool background = false;

    chain();
    ~chain();
};

// Constructor for chain struct
chain::chain() {
}

// Destructor for chain struct
chain::~chain() {
}

// Vector to hold chains
std::vector<chain*> cond_chain;

// Bool for pipe hygiene
bool is_read_end_open = false;

// Read end of the previous pipe
int read_end;

// Working directory for cd commands
std::string working_dir;

// COMMAND EXECUTION

// command::run()
//    Create a single child process running the command in `this`.
//    Sets `this->pid` to the pid of the child process and returns `this->pid`.
//
//    PART 1: Fork a child process and run the command using `execvp`.
//       This will require creating an array of `char*` arguments using
//       `this->args[N].c_str()`.
//    PART 5: Set up a pipeline if appropriate. This may require creating a
//       new pipe (`pipe` system call), and/or replacing the child process's
//       standard input/output with parts of the pipe (`dup2` and `close`).
//       Draw pictures!
//    PART 7: Handle redirections.
//    PART 8: Update the `command` structure and this function to support
//       setting the child process’s process group. To avoid race conditions,
//       this will require TWO calls to `setpgid`.

pid_t command::run(pid_t groupid) {
    assert(this->args.size() > 0);
    // Your code here!
    // Declare pipe FDs
    int pfd[2];
    // Declare vector for child's arguments and copy over from this->args using c_str()
    vector<char*> child_args;
    for (auto it = this->args.begin(); it != this->args.end(); it++) {
        child_args.push_back((char*) (*it).c_str());
    }
    // Add terminating null character to end arguments
    child_args.push_back(NULL);

    // Create pipe
    if (this->link == TYPE_PIPE) {
        int check = pipe(pfd);
        assert(check == 0);
    }

    // Fork child and if valid, run execvp with new argument vector
    pid_t p = fork();

    // Set appropriate group IDs
    if (p == 0) {
        setpgid(0, groupid);
    }
    else {
        setpgid(p, groupid);
    }

    // If pipe from previous command is open, read from it and clean up
    if (is_read_end_open) {
        if (p == 0) {
            dup2(read_end, 0);
            close(read_end);
        }
        if (p > 0) {
            close(read_end);
            is_read_end_open = false;
        }
    }

    // If this is a brand new pipe, set up fds and clean up
    if (this->link == TYPE_PIPE) {
        if (p == 0) {
            close(pfd[0]);
            dup2(pfd[1], 1);
            close(pfd[1]);
        }
        if (p > 0) {
            close(pfd[1]);
        }
        is_read_end_open = true;
        read_end = pfd[0];
    }

    // Check if redirection is required
    if (!this->input_file.empty() || !this->output_file.empty() || !this->error_file.empty()) {
        if (p == 0) {

            // Open input file in read-only mode and set up fds
            if (!this->input_file.empty()) {
                int fd_0 = open((const char*)this->input_file.c_str(), O_RDONLY, 0666);
                if (fd_0 == -1) {
                    fprintf(stderr, "No such file or directory \n");
                    _exit(1);
                }
                else {
                    dup2(fd_0, 0);
                    close(fd_0);
                }  
            }

            // Open output file in write-only mode (create if needed) and set up fds
            if (!this->output_file.empty()) {
                int fd_1 = open((const char*)this->output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd_1 == -1) {
                    fprintf(stderr, "No such file or directory \n");
                    _exit(1);
                }
                else {
                    dup2(fd_1, 1);
                    close(fd_1);
                }  
            }

            // Open error file in write-only mode (create if needed) and set up fds
            if (!this->error_file.empty()) {
                int fd_2 = open((const char*)this->error_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd_2 == -1) {
                    fprintf(stderr, "No such file or directory \n");
                    _exit(1);
                }
                else {
                    dup2(fd_2, 2);
                    close(fd_2);
                }  
            }
        }
    }

    // Child executes actual command
    if (p == 0) {

        // If this command is cd then exit with good status to keep chain moving
        // If child changes directory it will not affect parent
        if (this->is_cd && !working_dir.empty()) {
            _exit(0);
        }
        
        // Have future children change directory if needed for themselves
        if (!working_dir.empty()) {
            assert(chdir(working_dir.c_str()) == 0);
        }

        // Execute command
        if (execvp(child_args[0], child_args.data()) < 0) {
            _exit(1);
        }
        this->pid = getpid();
    }  
    return this->pid;
}

// run_conditional(chain* this_chain)
//   Run each conditional chain with forked child

void run_conditional(chain* this_chain) {

    // Set group pids
    setpgid(0, 0);
    // First command is start of chain
    command* ccur = this_chain->start;

    // Iterate through chain
    while (ccur != nullptr) {

        ccur->run(getpid());

        // If pipe then go through all commands in pipeline and run in parallel
        if (ccur->link == TYPE_PIPE) {
            while (ccur->link == TYPE_PIPE) {
                ccur = ccur->next;
                ccur->run(getpid());
            }
            // Wait on last command in pipeline
            waitpid(ccur->pid, &ccur->status, 0);
        }

        if (ccur->link == TYPE_AND) {
            // Wait for status of current command to update before proceeding
            waitpid(ccur->pid, &ccur->status, 0);
            // If failed then skip through the conditional
            if (!WIFEXITED(ccur->status) || WEXITSTATUS(ccur->status) != 0) {
                while (ccur->link != TYPE_OR && ccur->next != nullptr) {
                    ccur = ccur->next;
                }
            }
        }

        else if (ccur->link == TYPE_OR) {
            // Wait for status of current command to update before proceeding
            waitpid(ccur->pid, &ccur->status, 0);
            // If exited correctly then skip through the conditional
            if (WIFEXITED(ccur->status) && WEXITSTATUS(ccur->status) == 0) {
                while (ccur->link != TYPE_AND && ccur->next != nullptr) {
                    ccur = ccur->next;
                }
            }
        }

        // If no special operator then just wait normally
        else {
            waitpid(ccur->pid, &ccur->status, 0);
        }

        // Indicates end of this conditonal so break
        if (ccur->link == TYPE_BACKGROUND || ccur->link == TYPE_SEQUENCE) {
            break;
        }

        ccur = ccur->next;
    } 
}

// run_list(c)
//    Run the command *list* starting at `c`. Initially this just calls
//    `c->run()` and `waitpid`; you’ll extend it to handle command lists,
//    conditionals, and pipelines.
//
//    It is possible, and not too ugly, to handle lists, conditionals,
//    *and* pipelines entirely within `run_list`, but many students choose
//    to introduce `run_conditional` and `run_pipeline` functions that
//    are called by `run_list`. It’s up to you.
//
//    PART 1: Start the single command `c` with `c->run()`,
//        and wait for it to finish using `waitpid`.
//    The remaining parts may require that you change `struct command`
//    (e.g., to track whether a command is in the background)
//    and write code in `command::run` (or in helper functions).
//    PART 2: Treat background commands differently.
//    PART 3: Introduce a loop to run all commands in the list.
//    PART 4: Change the loop to handle conditionals.
//    PART 5: Change the loop to handle pipelines. Start all processes in
//       the pipeline in parallel. The status of a pipeline is the status of
//       its LAST command.
//    PART 8: - Make sure every process in the pipeline has the same
//         process group, `pgid`.
//       - Call `claim_foreground(pgid)` before waiting for the pipeline.
//       - Call `claim_foreground(0)` once the pipeline is complete.


void run_list(command* ccur) {

    (void) ccur;

    // Hold value of current chain
    chain* curr_chain;

    // Iterate through every chain
    for (size_t i = 0; i != cond_chain.size(); i++) {
        curr_chain = cond_chain[i];

        // Fork child to run each conditional
        pid_t sub = fork();

        // Child runs this
        if (sub == 0) {
            run_conditional(curr_chain);
            _exit(0);
        }

        // Parent waits if foreground chain
        if (!curr_chain->background) {
            if (sub > 0) {
                claim_foreground(sub);
                int parent_stat;
                waitpid(sub, &parent_stat, 0);
                claim_foreground(0);
            }
        }
    }  
}

// parse_line(s)
//    Parse the command list in `s` and return it. Returns `nullptr` if
//    `s` is empty (only spaces). You’ll extend it to handle more token
//    types.

command* parse_line(const char* s) {
    shell_parser parser(s);
    // Your code here!

    // Build the command
    // The handout code treats every token as a normal command word.
    // You'll add code to handle operators.
    // Linked list structure from section
    command* chead = nullptr;    // first command in list
    command* clast = nullptr;    // last command in list
    command* ccur = nullptr;     // current command being built

    // Clear vector of conditional chains
    cond_chain.clear();

    // Initialize first chain
    chain* old_chain = new chain;

    // Bools for redirection
    bool input_file = false;
    bool output_file = false;
    bool error_file = false;

    for (shell_token_iterator it = parser.begin(); it != parser.end(); ++it) {
        
        
        // Execute different blocks based on operators
        switch (it.type()) {
        case TYPE_NORMAL: 
            // Create at least one new command
            if (!ccur) {
                ccur = new command;
                // Set to first command in chain
                if (!old_chain->start) {
                    old_chain->start = ccur;
                    cond_chain.push_back(old_chain);
                }
                if (clast) {
                    clast->next = ccur;
                    ccur->prev = clast;
                } else {
                chead = ccur;
                }
            }

            // If "cd" then toggle boolean
            if (it.str().compare(std::string("cd")) == 0) {
                ccur->is_cd = true;
                ccur->args.push_back(it.str());
                break;
            }
            
            // If any redirections needed from previous token, add this token as file name
            else if (input_file) {
                ccur->input_file = it.str();
                input_file = false;
            }
            else if (output_file) {
                ccur->output_file = it.str();
                output_file = false;
            }
            else if (error_file) {
                ccur->error_file = it.str();
                error_file = false;
            }

            // Add cd args to command but if valid path then add to global working_dir
            else if (ccur->is_cd) {
                ccur->args.push_back(it.str());

                // Check is this is an actual path (from Stack Overflow)
                struct stat check;
                if (stat(it.str().c_str(), &check) == 0) {
                    working_dir = it.str();
                }
            }
            else {
                ccur->args.push_back(it.str());
            }

            break;
        case TYPE_BACKGROUND: 
            // Create new chain and command
            assert(ccur);
            clast = ccur;
            clast->link = it.type();
            // Set that last chain was background
            old_chain->background = true;
            if (it != parser.end()) {
                chain* new_chain = new chain;
                old_chain = new_chain;
            }
            ccur = nullptr;
            break;
        case TYPE_SEQUENCE: 
            // Create new chain and end command
            assert(ccur);
            clast = ccur;
            clast->link = it.type();
            // Set that last chain was foreground
            old_chain->background = false;
            if (it != parser.end()) {
                chain* new_chain = new chain;
                old_chain = new_chain;
            }
            ccur = nullptr;
            break;
        case TYPE_AND: case TYPE_OR:
            // Set link and end command
            assert(ccur);
            clast = ccur;
            clast->link = it.type();
            ccur = nullptr;
            break;
        case TYPE_PIPE:
            // Set link and end command
            assert(ccur);
            clast = ccur;
            clast->link = it.type();
            ccur = nullptr;
            break;
        case TYPE_REDIRECT_OP:
            // Toggle booleans so next token can be added as file name
            if (it.str().compare(std::string(">")) == 0) {
                output_file = true;
            }
            else if (it.str().compare(std::string("<")) == 0) {
                input_file = true;
            }
            else if (it.str().compare(std::string("2>")) == 0) {
                error_file = true;
            }
            break;
        }
    }

    // Take care of memory leaks
    delete[] clast;
    delete[] ccur;
    delete[] old_chain;
    delete[] new_chain;

    return chead;
}


int main(int argc, char* argv[]) {
    FILE* command_file = stdin;
    bool quiet = false;

    // Check for `-q` option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = true;
        --argc, ++argv;
    }

    // Check for filename option: read commands from file
    if (argc > 1) {
        command_file = fopen(argv[1], "rb");
        if (!command_file) {
            perror(argv[1]);
            return 1;
        }
    }

    // - Put the shell into the foreground
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    claim_foreground(0);
    set_signal_handler(SIGTTOU, SIG_IGN);

    // Signal handler for control-C
    set_signal_handler(SIGTERM, handle_sig);

    char buf[BUFSIZ];
    int bufpos = 0;
    bool needprompt = true;

    while (!feof(command_file)) {
        // Print the prompt at the beginning of the line
        if (needprompt && !quiet) {
            printf("sh61[%d]$ ", getpid());
            fflush(stdout);
            needprompt = false;
        }

        // If signal toggled, make new prompt
        if (term_signal) {
            needprompt = true;
            term_signal = 0;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == nullptr) {
            if (ferror(command_file) && errno == EINTR) {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            } else {
                if (ferror(command_file)) {
                    perror("sh61");
                }
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
            if (command* c = parse_line(buf)) {
                run_list(c);
                delete c;
            }
            bufpos = 0;
            needprompt = 1;
        }

        // Handle zombie processes and/or interrupt requests
        // Your code here!

        // Waitpid with -1 to wait for all child processes
        int temp_status;
        while (waitpid(-1, &temp_status, WNOHANG) > 0);
    }

    return 0;
}
