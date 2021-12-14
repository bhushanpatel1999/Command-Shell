#include "sh61.hh"
#include <cstring>
#include <cerrno>
#include <vector>
#include <sys/stat.h>
#include <sys/wait.h>

using namespace std;
// For the love of God
#undef exit
#define exit __DO_NOT_CALL_EXIT__READ_PROBLEM_SET_DESCRIPTION__

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

    pid_t run();
    // type of connection to next command in linked list
    int link; 

    // // Bool to know when to stop background chain
    // bool
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


struct chain {
    command* start = nullptr;
    command* end = nullptr;

    bool background = false;

    chain();
    ~chain();
};

chain::chain() {
}

chain::~chain() {
}
// Vector to hold commands that start a background chain
std::vector<chain*> cond_chain;

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

pid_t command::run() {
    assert(this->args.size() > 0);
    // Your code here!
    // Declare vector for child's arguments and copy over from this->args using c_str()
    vector<char*> child_args;
    for (auto it = this->args.begin(); it != this->args.end(); it++) {
        child_args.push_back((char*) (*it).c_str());
    }
    // Add terminating null character to end arguments
    child_args.push_back(NULL);
    // Fork child and if valid, run execvp with new argument vector
    pid_t p = fork();
    if (p == 0) {
        if (execvp(child_args[0], child_args.data()) < 0) {
            _exit(1);
        }
        this->pid = getpid();
    }  

    // fprintf(stderr, "command::run not done yet\n");
    return this->pid;
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


void run_conditional(chain* this_chain) {

    command* ccur = this_chain->start;
    while (ccur != nullptr) {
        // fprintf(stderr, "command::run not done yet\n");
        // Next operator is ";" so run in foreground
        // Traverse linked list
        ccur->run();

        // if (ccur->link == TYPE_SEQUENCE) {
        //     // Wait for status of current command to update before proceeding
        // }

        // else if (ccur->link == TYPE_BACKGROUND) {
        //     // Wait for status of current command to update before proceeding
        //     // waitpid(ccur->pid, &ccur->status, 0);
        //     // if (!WIFEXITED(ccur->status)) {
        //     //     fprintf(stderr, "Child did not exit correctly.\n");
        //     // }
        //     ccur = ccur->next;
        // }


        if (ccur->link == TYPE_AND) {
            // Wait for status of current command to update before proceeding
            waitpid(ccur->pid, &ccur->status, 0);
            if (!WIFEXITED(ccur->status)) {
                fprintf(stderr, "Child did not exit correctly.\n");
            }
            else if (WEXITSTATUS(ccur->status) != 0) {
                while (ccur->link != TYPE_OR && ccur->next != nullptr) {
                    ccur = ccur->next;
                }
            }
        }

        else if (ccur->link == TYPE_OR) {
            // Wait for status of current command to update before proceeding
            waitpid(ccur->pid, &ccur->status, 0);
            if (!WIFEXITED(ccur->status)) {
                fprintf(stderr, "Child did not exit correctly.\n");
            }
            else if (WEXITSTATUS(ccur->status) == 0) {
                while (ccur->link != TYPE_AND && ccur->next != nullptr) {
                    ccur = ccur->next;
                }
            }
        }

        else {
            waitpid(ccur->pid, &ccur->status, 0);
            if (!WIFEXITED(ccur->status)) {
                fprintf(stderr, "Child did not exit correctly.\n");
            }
        }

        if (ccur->link == TYPE_BACKGROUND || ccur->link == TYPE_SEQUENCE) {
            break;
        }

        ccur = ccur->next;
    } 
}

void run_list(command* ccur) {

    chain* curr_chain;
    command* cend;

    for (size_t i = 0; i != cond_chain.size(); i++) {
        curr_chain = cond_chain[i];
        if (!curr_chain->background) {
            run_conditional(curr_chain);
        }
        else {
            pid_t childpid = fork();
            if (childpid == 0) {
                run_conditional(curr_chain);
                _exit(0);
            }
        }
    }  

    // while (ccur != nullptr) {
    //     // fprintf(stderr, "command::run not done yet\n");
    //     // Next operator is ";" so run in foreground
    //     // Traverse linked list
    //     ccur->run();

    //     if (ccur->link == TYPE_SEQUENCE) {
    //         // Wait for status of current command to update before proceeding
    //         waitpid(ccur->pid, &ccur->status, 0);
    //         if (!WIFEXITED(ccur->status)) {
    //             fprintf(stderr, "Child did not exit correctly.\n");
    //         }
    //         ccur = ccur->next;
    //     }

    //     else if (ccur->link == TYPE_BACKGROUND) {
    //         // Wait for status of current command to update before proceeding
    //         // waitpid(ccur->pid, &ccur->status, 0);
    //         // if (!WIFEXITED(ccur->status)) {
    //         //     fprintf(stderr, "Child did not exit correctly.\n");
    //         // }
    //         ccur = ccur->next;
    //     }

    //     else if (ccur->link == TYPE_AND) {
    //         // Wait for status of current command to update before proceeding
    //         waitpid(ccur->pid, &ccur->status, 0);
    //         if (!WIFEXITED(ccur->status)) {
    //             fprintf(stderr, "Child did not exit correctly.\n");
    //         }
    //         else if (WEXITSTATUS(ccur->status) != 0) {
    //             while (ccur->link != TYPE_OR && ccur->next != nullptr) {
    //                 ccur = ccur->next;
    //             }
    //         }
    //     }

    //     else if (ccur->link == TYPE_OR) {
    //         // Wait for status of current command to update before proceeding
    //         waitpid(ccur->pid, &ccur->status, 0);
    //         if (!WIFEXITED(ccur->status)) {
    //             fprintf(stderr, "Child did not exit correctly.\n");
    //         }
    //         else if (WEXITSTATUS(ccur->status) == 0) {
    //             while (ccur->link != TYPE_AND && ccur->next != nullptr) {
    //                 ccur = ccur->next;
    //             }
    //         }
    //     }
    //     ccur = ccur->next;
    // }
}

// run_conditional(ccur)
//    Run the chain of commands that should be ran in the background


// void run_conditional(command* cstart) {

// }

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
    cond_chain.clear();
    chain* old_chain = new chain;

    for (shell_token_iterator it = parser.begin(); it != parser.end(); ++it) {
        

        // Create at least one new command
        if (!ccur) {
            ccur = new command;
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

        
        // Execute different blocks based on operators
        switch (it.type()) {
        case TYPE_NORMAL: 
            ccur->args.push_back(it.str());
            if (it == parser.end()) {
                old_chain->end = ccur;
            }
            break;
        case TYPE_BACKGROUND: 
            assert(ccur);
            clast = ccur;
            clast->link = it.type();
            old_chain->end = ccur;
            old_chain->background = true;
            if (it != parser.end()) {
                chain* new_chain = new chain;
                old_chain = new_chain;
            }
            ccur = nullptr;
            break;
        case TYPE_SEQUENCE: 
            assert(ccur);
            clast = ccur;
            clast->link = it.type();
            old_chain->end = ccur;
            old_chain->background = false;
            if (it != parser.end()) {
                chain* new_chain = new chain;
                old_chain = new_chain;
            }
            ccur = nullptr;
            break;
        case TYPE_AND: case TYPE_OR:
            assert(ccur);
            clast = ccur;
            clast->link = it.type();
            ccur = nullptr;
            break;
        }
    }
    //fprintf(stderr, "Size of chain: %ld\n", cond_chain.size());
    // chain* curr_chain;
    // command* cend;
    // for (size_t i = 0; i != cond_chain.size(); i++) {
    //     curr_chain = cond_chain[i];
    //     cend = curr_chain->end;
    //     //cend->next = nullptr;
    //     fprintf(stderr, "Next: %p\n", cend->next);
    // }
    // ccur = chead;
    // // int count = 0;
    // while (ccur != nullptr) {
    //     // count++;
    //     if (ccur->link == TYPE_BACKGROUND || ccur->link == TYPE_SEQUENCE) {
    //         ccur = ccur->next;
    //         ccur->prev->next == nullptr;
    //     }
    //     else {
    //         ccur = ccur->next;
    //     }
    // }
    // fprintf(stderr, "Count: %d", count);
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
    }

    return 0;
}
