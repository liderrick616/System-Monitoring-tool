
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <utmp.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <utmpx.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
void saveCursorPosition(void) {
    printf("\0337");
}
void restoreCursorPosition(void) {
    printf("\0338");
}
void sigint_handler(int signum) { //ctrlc
    saveCursorPosition();
    char response[4]; // Include space for 'yes\n' and null terminator
    printf("\nDo you want to quit? [yes/no]: ");
    fflush(stdout);
    if (fgets(response, sizeof(response), stdin)) {
        if (strncmp(response, "yes", 3) == 0) {
            exit(0); // Exit the program if the user confirms
        }
        else {
            if (signal(SIGINT, sigint_handler) == SIG_ERR) {
                perror("sig err");
                exit(1);
            }
            while (strchr(response, '\n') == NULL) {
                fgets(response, sizeof(response), stdin);
            }
            restoreCursorPosition();
       }
    }
    else {
        // If fgets fails, reset the handler as well
        if (signal(SIGINT, sigint_handler) == SIG_ERR) {
            perror("sig err");
            exit(1);
        }
    }
    while (strchr(response, '\n') == NULL) {
        fgets(response, sizeof(response), stdin);
    }
    restoreCursorPosition();
}
void handle_sigtstp(int signum) { //ctrlz
    if (signal(signum, handle_sigtstp) == SIG_ERR) {
        perror("sig err");
        exit(1);
    }
    //fflush(stdout);
}
void setup_signal_handlers() {
    struct sigaction sa_int, sa_tstp;
    // Setup for SIGINT (Ctrl-C)
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0; // Optionally add SA_RESTART to make certain interrupted system calls to restart
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        perror("Failed to set SIGINT handler");
        exit(EXIT_FAILURE);
    }
    // Setup for SIGTSTP (Ctrl-Z)
    sa_tstp.sa_handler = handle_sigtstp;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = 0;
    if (sigaction(SIGTSTP, &sa_tstp, NULL) == -1) {
        perror("Failed to set SIGTSTP handler");
        exit(EXIT_FAILURE);
    }
}
typedef struct {
    int system_flag;
    int user_flag;
    int graphics_flag;
    int sequential_flag;
    int sample_count;
    int time_delay;
} ProgramConfig;

void initialize_config(ProgramConfig *config);
void gather_cpu_info(char *cpu_info_buffer, int buffer_size, int tdelay, double *cpu_usage);
void gather_memory_info(long *phys_used_mem, long *total_phys_mem, long *virt_used_mem, long *total_virt_mem, char *memory_info_buffer, size_t buffer_size);
void read_uptime(char *uptime_str, size_t max_size);
//void list_user_sessions(int *session_count);
void list_user_sessions(int *session_count, char *users_info_buffer, size_t buffer_size);
void list_user(int *session_count);
//void display_system_info(const ProgramConfig *config);
void display_system_info(const ProgramConfig *config, char *cpu_info_buffer,char *memory_info_buffer,char *users_info_buffer);
void display_system_info_system(const ProgramConfig *config, char *cpu_info_buffer,char *memory_info_buffer);
//void display_system_info_system(const ProgramConfig *config);
//void display_system_info_sequential(const ProgramConfig *config);
void display_system_info_sequential(const ProgramConfig *config, char *cpu_info_buffer,char *memory_info_buffer,char *users_info_buffer);
//void display_user_info(const ProgramConfig *config);
void display_user_info(const ProgramConfig *config,char *users_info_buffer);
/*typedef struct {
    int current_index;
} UsageHistory;
UsageHistory usage_history;*/
void initialize_usage_history();
//void graphical_system_info(const ProgramConfig *config);
void graphical_system_info(const ProgramConfig *config, char *cpu_info_buffer,char *memory_info_buffer,char *users_info_buffer);
void parse_arguments(int argc, char *argv[], ProgramConfig *config) {
    int arg_index = 1; // Start from the first argument after the program name

    // Check and parse the first and second arguments as sample_count and time_delay if they are numbers
    if (argc > 1 && argv[arg_index][0] != '-') {
        // If the first argument is a number, update sample_count
        if (sscanf(argv[arg_index], "%d", &config->sample_count) == 1) {
            arg_index++; // Increment index if sample_count is successfully parsed
        }
        // If the next argument is a number, update time_delay
        if (arg_index < argc && sscanf(argv[arg_index], "%d", &config->time_delay) == 1) {
            arg_index++; // Increment index if time_delay is successfully parsed
        }
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--system") == 0) {
            config->system_flag = 1;
        } else if (strcmp(argv[i], "--user") == 0) {
            config->user_flag = 1;
        } else if (strcmp(argv[i], "--graphics") == 0) {
            config->graphics_flag = 1;
        } else if (strcmp(argv[i], "--sequential") == 0) {
            config->sequential_flag = 1;
        }
    }
}

void clear_screen() {
    printf("\033[H\033[J"); // Clears the screen and moves cursor to top-left
}

int main(int argc, char *argv[]) {
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        perror("sig err");
        exit(1);
    }

    // Set the SIGTSTP (Ctrl-Z) handler
    if (signal(SIGTSTP, handle_sigtstp) == SIG_ERR) {
        perror("sig err");
        exit(1);
    }
    ProgramConfig config;
    initialize_config(&config);
    // Parse command line arguments
    parse_arguments(argc, argv, &config);
    // Clear the screen before displaying information
    clear_screen();
    int pipe_cpu[2], pipe_memory[2], pipe_users[2];
    // Create pipes
    if (pipe(pipe_cpu) == -1 || pipe(pipe_memory) == -1 || pipe(pipe_users) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    // CPU Utilization process

    pid_t pid_cpu = fork();
    if (pid_cpu == 0) { // Child process for CPU Utilization
        close(pipe_cpu[0]); // Close unused read end
        double cpu_usage;
        char cpu_info_buffer[256];
        gather_cpu_info(cpu_info_buffer, sizeof(cpu_info_buffer), config.time_delay, &cpu_usage);
        write(pipe_cpu[1], cpu_info_buffer, strlen(cpu_info_buffer)); // Send CPU info to parent
        close(pipe_cpu[1]); // Close write end
        exit(EXIT_SUCCESS);
    }
    // Memory Utilization process
    pid_t pid_memory = fork();
    if (pid_memory == 0) { // Child process for Memory Utilization
        close(pipe_memory[0]); // Close unused read end
        long phys_used_mem, total_phys_mem, virt_used_mem, total_virt_mem;
        char memory_info_buffer[256];
        gather_memory_info(&phys_used_mem, &total_phys_mem, &virt_used_mem, &total_virt_mem, memory_info_buffer, sizeof(memory_info_buffer));
        write(pipe_memory[1], memory_info_buffer, strlen(memory_info_buffer)); // Send Memory info to parent
        close(pipe_memory[1]); // Close write end
        exit(EXIT_SUCCESS);
    }
    // User Sessions process
    pid_t pid_users = fork();
    if (pid_users == 0) { // Child process for User Sessions
        close(pipe_users[0]); // Close unused read end
        char users_info_buffer[256];
        int user_session_count = 0;
        list_user_sessions(&user_session_count,users_info_buffer, sizeof(users_info_buffer));
        write(pipe_users[1], users_info_buffer, strlen(users_info_buffer)); // Send Users info to parent
        printf("\033[H\033[J");
        close(pipe_users[1]); // Close write end
        exit(EXIT_SUCCESS);
    }
    signal(SIGINT,sigint_handler);
    signal(SIGTSTP, handle_sigtstp);
    close(pipe_cpu[1]); // Close unused write ends
    close(pipe_memory[1]);
    close(pipe_users[1]);

    waitpid(pid_cpu,NULL,0);        // Wait for the child process to exit
    waitpid(pid_memory,NULL,0); // wait for the child process of memory
    waitpid(pid_users,NULL,0); //
    // Parent process: read and display information from child processes
    char read_buffer_cpu[256]; // Buffer for reading pipe data
    char read_buffer_mem[256];
    char read_buffer_user[256];

// Read CPU info from the pipe
    ssize_t num_read_cpu = read(pipe_cpu[0], read_buffer_cpu, sizeof(read_buffer_cpu) - 1);
    if (num_read_cpu > 0) {
        read_buffer_cpu[num_read_cpu] = '\0'; // Null-terminate the string
        printf("CPU Info: %s\n", read_buffer_cpu);
    } else if (num_read_cpu == -1) {
        perror("Failed to read from CPU pipe");
    }
// Read Memory info from the pipe
    ssize_t num_read_mem = read(pipe_memory[0], read_buffer_mem, sizeof(read_buffer_mem) - 1);
    if (num_read_mem > 0) {
        read_buffer_mem[num_read_mem] = '\0'; // Null-terminate the string
        printf("Memory Info: %s\n", read_buffer_mem);
    } else if (num_read_mem == -1) {
        perror("Failed to read from Memory pipe");
    }
// Read User Sessions info from the pipe
    ssize_t num_read_user = read(pipe_users[0], read_buffer_user, sizeof(read_buffer_user) - 1);
    if (num_read_user > 0) {
        read_buffer_user[num_read_user] = '\0'; // Null-terminate the string
        //printf("User Sessions Info: %s\n", read_buffer_user);
    } else if (num_read_user == -1) {
        perror("Failed to read from User Sessions pipe");
    }

    if (argc == 1) {
        display_system_info(&config,read_buffer_cpu,read_buffer_mem,read_buffer_user);
    }
    // Display system information if the flag is set
    if (config.system_flag) {
        display_system_info_system(&config,read_buffer_cpu,read_buffer_mem);
    }

    // Display graphical system information if the flag is set
    if (config.graphics_flag) {
        graphical_system_info(&config,read_buffer_cpu,read_buffer_mem,read_buffer_user);
    }

    // Additional functionality for other flags can be added here
    if (config.user_flag) {
        display_user_info(&config,read_buffer_user);
    }
    if (config.sequential_flag){
        display_system_info_sequential(&config,read_buffer_cpu,read_buffer_mem,read_buffer_user);
    }
    // If only sample_count and time_delay are provided as arguments
    if (argc == 3 && !config.system_flag && !config.graphics_flag && !config.user_flag && !config.sequential_flag) {
        //display_system_info(&config);
        display_system_info(&config,read_buffer_cpu,read_buffer_mem,read_buffer_user);
        //return 0; // Exit after displaying the default system info
    }
    if (argc == 2 && !config.system_flag && !config.graphics_flag && !config.user_flag && !config.sequential_flag) {
        config.time_delay = 1;
        display_system_info(&config,read_buffer_cpu,read_buffer_mem,read_buffer_user);
        //display_system_info(&config);
        //return 0; // Exit after displaying the default system info
    }
    close(pipe_users[0]);
    close(pipe_cpu[0]);
    close(pipe_memory[0]);
}

