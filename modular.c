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
#include <sys/wait.h>
#include <sys/resource.h>

typedef struct {
    int system_flag;
    int user_flag;
    int graphics_flag;
    int sequential_flag;
    int sample_count;
    int time_delay;
} ProgramConfig;

void initialize_config(ProgramConfig *config) {
    config->system_flag = 0;
    config->user_flag = 0;
    config->graphics_flag = 0;
    config->sequential_flag = 0;
    config->sample_count = 10;
    config->time_delay = 1; // Default value
}
void gather_cpu_info(char *cpu_info_buffer, int buffer_size, int tdelay, double *cpu_usage) {
    FILE *fp;
    char buffer[256];
    unsigned long long int fields[8], total_tick[2], total_idle[2], total_usage[2];
    //double cpu_usage;

    // Take the first sample
    fp = fopen("/proc/stat", "r");
    if (!fp) {
        snprintf(cpu_info_buffer, buffer_size, "Error: Failed to open /proc/stat\n");
        return;
    }
    fgets(buffer, sizeof(buffer), fp);
    fclose(fp);
    sscanf(buffer, "cpu %llu %llu %llu %llu %llu %llu %llu",
           &fields[0], &fields[1], &fields[2], &fields[3],
           &fields[4], &fields[5], &fields[6]);
    total_tick[0] = fields[0] + fields[1] + fields[2] + fields[3] + fields[4] + fields[5] + fields[6];
    total_idle[0] = fields[3]; // idle ticks count

    // Wait for tdelay seconds
    sleep(tdelay);

    // Take the second sample
    fp = fopen("/proc/stat", "r");
    if (!fp) {
        snprintf(cpu_info_buffer, buffer_size, "Error: Failed to open /proc/stat the second time\n");
        return;
    }
    fgets(buffer, sizeof(buffer), fp);
    fclose(fp);
    sscanf(buffer, "cpu %llu %llu %llu %llu %llu %llu %llu",
           &fields[0], &fields[1], &fields[2], &fields[3],
           &fields[4], &fields[5], &fields[6]);
    total_tick[1] = fields[0] + fields[1] + fields[2] + fields[3] + fields[4] + fields[5] + fields[6];
    total_idle[1] = fields[3]; // idle ticks count

    // Calculate CPU usage
    total_usage[0] = total_tick[0] - total_idle[0];
    total_usage[1] = total_tick[1] - total_idle[1];
    *cpu_usage = 100.0 * (total_usage[1] - total_usage[0]) / (total_tick[1] - total_tick[0]);

    // Store the calculated CPU usage in the provided buffer
    //snprintf(cpu_info_buffer, buffer_size, "\r total cpu use =: %.2f%%\n", cpu_usage);
    //sscanf(cpu_info_buffer,  "\r total cpu use =: %lf%%\n", &cpu_usage);
}

void gather_memory_info(long *phys_used_mem, long *total_phys_mem, long *virt_used_mem, long *total_virt_mem, char *memory_info_buffer, size_t buffer_size) {
    FILE *fp;
    char buffer[256];
    char *endptr;
    long mem_total = 0, mem_free = 0, buffers = 0, cached = 0, swap_total = 0, swap_free = 0;
    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        perror("Failed to open /proc/meminfo");
        return;
    }

    while (fgets(buffer, sizeof(buffer), fp)) {
        // Skipping lines that are not required
        if (strncmp(buffer, "VmallocTotal:", 13) == 0) {
            continue; // Skip this line
        }
        if (strncmp(buffer, "MemTotal:", 9) == 0) {
            mem_total = strtol(buffer + 9, &endptr, 10);
        } else if (strncmp(buffer, "MemFree:", 8) == 0) {
            mem_free = strtol(buffer + 8, &endptr, 10);
        } else if (strncmp(buffer, "Buffers:", 8) == 0) {
            buffers = strtol(buffer + 8, &endptr, 10);
        } else if (strncmp(buffer, "Cached:", 7) == 0) {
            cached = strtol(buffer + 7, &endptr, 10);
        } else if (strncmp(buffer, "SwapTotal:", 10) == 0) {
            swap_total = strtol(buffer + 10, &endptr, 10);
        } else if (strncmp(buffer, "SwapFree:", 9) == 0) {
            swap_free = strtol(buffer + 9, &endptr, 10);
        }
        // Check for conversion errors
        if (*endptr && *endptr != ' ' && *endptr != '\n') {
            fprintf(stderr, "Error parsing memory info: %s", buffer);
            fclose(fp);
            return;
        }
    }
    fclose(fp);

    *total_phys_mem = mem_total;
    *phys_used_mem = mem_total - mem_free - buffers - cached;
    *total_virt_mem = swap_total;
    *virt_used_mem = swap_total - swap_free;
    //snprintf(memory_info_buffer, buffer_size, "Physical Used: %ld kB, Total Physical: %ld kB, Virtual Used: %ld kB, Total Virtual: %ld kB", *phys_used_mem, *total_phys_mem, *virt_used_mem, *total_virt_mem);
}
void read_uptime(char *uptime_str, size_t max_size) {
    FILE *fp;
    char buffer[256];
    double uptime_seconds;
    char *endptr;

    fp = fopen("/proc/uptime", "r");
    if (fp == NULL) {
        perror("Failed to open /proc/uptime");
        snprintf(uptime_str, max_size, "Unknown");
        return;
    }

    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        uptime_seconds = strtod(buffer, &endptr);

        // Check for conversion errors
        if (*endptr && *endptr != ' ' && *endptr != '\n') {
            fprintf(stderr, "Error parsing uptime: %s", buffer);
            snprintf(uptime_str, max_size, "Unknown");
            fclose(fp);
            return;
        }
    } else {
        fprintf(stderr, "Failed to read /proc/uptime\n");
        snprintf(uptime_str, max_size, "Unknown");
        fclose(fp);
        return;
    }
    fclose(fp);
    int days = (int)uptime_seconds / (24 * 3600);
    uptime_seconds -= days * (24 * 3600);
    int hours = (int)uptime_seconds / 3600;
    uptime_seconds -= hours * 3600;
    int minutes = (int)uptime_seconds / 60;
    int seconds = (int)uptime_seconds % 60;

    // Format the uptime string to include days, hours, minutes, and seconds
    snprintf(uptime_str, max_size, "%d days %02d:%02d:%02d (%d:%02d:%02d)", days, hours, minutes, seconds, hours + (days * 24), minutes, seconds);
}

void list_user_sessions(int *session_count, char *users_info_buffer, size_t buffer_size) {
    struct utmpx *entry;
    int count = 0;
    size_t current_pos = 0; // Current position in the buffer

    // Initialize the buffer
    if (users_info_buffer != NULL && buffer_size > 0) {
        users_info_buffer[0] = '\0';
    }

    // Set the utmp file to the beginning
    setutxent();

    // Read entries from the utmp file
    while ((entry = getutxent()) != NULL) {
        // Check if the entry type is USER_PROCESS (a user login)
        if (entry->ut_type == USER_PROCESS) {
            // Print directly to stdout
            printf("%s\t%s", entry->ut_user, entry->ut_line);
            int written = 0; // Bytes written by snprintf

            // Try to format into buffer if space allows
            if (current_pos < buffer_size) {
                written = snprintf(users_info_buffer + current_pos, buffer_size - current_pos, "%s\t%s",
                                   entry->ut_user, entry->ut_line);
                if (written > 0) current_pos += written;
            }

            // Check if the host field is not empty
            if (strlen(entry->ut_host) > 0) {
                printf("\t(%s)", entry->ut_host);
                // Also format into buffer if space allows
                if (current_pos < buffer_size) {
                    written = snprintf(users_info_buffer + current_pos, buffer_size - current_pos, "\t(%s)",
                                       entry->ut_host);
                    if (written > 0) current_pos += written;
                }
            }
            printf("\n");
            if (current_pos < buffer_size) {
                written = snprintf(users_info_buffer + current_pos, buffer_size - current_pos, "\n");
                if (written > 0) current_pos += written;
            }

            count++; // Increment the counter for each user session
        }
    }

    // Close the utmp file
    endutxent();

    // Store the count in the provided address
    if (session_count != NULL) {
        *session_count = count;
    }
}
void list_user(int *session_count) {
    struct utmpx *entry;
    int count = 0;
    // Set the utmp file to the beginning
    setutxent();

    // Read entries from the utmp file
    while ((entry = getutxent()) != NULL) {
        // Check if the entry type is USER_PROCESS (a user login)
        if (entry->ut_type == USER_PROCESS) {
            //printf("%s\t%s", entry->ut_user, entry->ut_line);

            // Check if the host field is not empty
            if (strlen(entry->ut_host) > 0) {
                //printf("\t(%s)", entry->ut_host);
            }
            count++; // Increment the counter for each user session
            //printf("\n");
        }
    }

    // Close the utmp file
    endutxent();
    // Store the count in the provided address
    if (session_count != NULL) {
        *session_count = count;
    }
}
//, char *cpu_info_buffer,char *memory_info_buffer,char *users_info_buffer
void display_system_info(const ProgramConfig *config, char *cpu_info_buffer,char *memory_info_buffer,char *users_info_buffer) {
    printf("\033[H\033[J");
    char memory_lines[config->sample_count][128]; // Array to store memory info strings
    FILE *fp;
    char line[128];
    long memory_usage = 0;
    char *endptr;

    fp = fopen("/proc/self/status", "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                memory_usage = strtol(line + 6, &endptr, 10);
                // Check for conversion errors
                if (*endptr != '\0' && *endptr != ' ' && *endptr != '\n') {
                    fprintf(stderr, "Error parsing memory usage: %s", line);
                    fclose(fp);
                    return;
                }
                break;
            }
        }
        fclose(fp);
    }
    struct utsname sys_info;
    uname(&sys_info);
    char uptime_str[100];
    read_uptime(uptime_str, sizeof(uptime_str));
    // Print static information first
    printf("Nbr of samples: %d -- every %d secs\n", config->sample_count, config->time_delay);
    printf(" Memory usage of this tool: %ld kB\n", memory_usage);
    printf("---------------------------------------\n");
    printf("### Memory ### (Phys.Used/Tot -- Virtual Used/Tot)\n");

    for (int i = 0; i < config->sample_count; i++) {
        printf("\n"); // Print placeholder lines for memory information
    }

    printf("---------------------------------------\n");
    printf("### Sessions/users ###\n");
    int user_session_count = 0;
    //char users_info_buffer[4096];
    list_user_sessions(&user_session_count,users_info_buffer, sizeof(users_info_buffer));
    printf("---------------------------------------\n");
    printf("Number of cores: %ld\n", sysconf(_SC_NPROCESSORS_ONLN));
    double cpu_usage_1;
    //char cpu_info_buffer[256];
    gather_cpu_info(cpu_info_buffer, sizeof(cpu_info_buffer),config->time_delay,&cpu_usage_1);
    printf("total cpu use = %.2f%%\n", cpu_usage_1);

    // Move the cursor up to the start of the memory information section
    printf("\033[%dA", config->sample_count + 5 + user_session_count); //change this later
    // Update only the memory information section in each loop
    for (int i = 0; i < config->sample_count; i++) {
        double cpu_usage;
        long phys_used_mem, total_phys_mem, virt_used_mem, total_virt_mem;
        gather_cpu_info(cpu_info_buffer, sizeof(cpu_info_buffer),config->time_delay, &cpu_usage);
        //char memory_info_buffer[256];
        gather_memory_info(&phys_used_mem, &total_phys_mem, &virt_used_mem, &total_virt_mem, memory_info_buffer, sizeof(memory_info_buffer));
        snprintf(memory_lines[i % config->sample_count], sizeof(memory_lines[0]),
                 "%.2f GB / %.2f GB -- %.2f GB / %.2f GB",
                 (double)phys_used_mem / 1024.0 / 1024, (double)total_phys_mem / 1024.0 / 1024,
                 ((double)virt_used_mem / 1024.0 / 1024)+(double)phys_used_mem / 1024.0 / 1024, (double)total_virt_mem / 1024.0 / 1024+(double)total_phys_mem / 1024.0 / 1024);

        // Clear and reprint memory lines
        for (int j = 0; j < config->sample_count; j++) {
            printf("\033[K"); // Clear the line
            if (j <= i) {
                printf("%s\n", memory_lines[j]);
            } else {
                printf("\n");
            }
        }
        //sleep((config->time_delay)*0.9);
        gather_cpu_info(cpu_info_buffer, sizeof(cpu_info_buffer),config->time_delay, &cpu_usage);
        printf("\033[%dB",4+user_session_count);
        printf("\033[2K");
        printf("\rtotal cpu use = %.2f%%", cpu_usage);
        printf("\033[%dA",4+user_session_count);
        printf("\033[%dD",21);

        // Position cursor for next update
        if (i < config->sample_count-1) {
            printf("\033[%dA", config->sample_count);
            //sleep(config->time_delay);
        }
        else {
            printf("\033[%dB", 5+user_session_count);
        }
        //sleep((config->time_delay));
        sleep((config->time_delay));
    }
    printf("---------------------------------------\n");
    printf("### System Information ###\n");
    read_uptime(uptime_str, sizeof(uptime_str));
    printf(" System Name = %s\n", sys_info.sysname);
    printf(" Machine Name = %s\n", sys_info.nodename);
    printf(" Version = %s\n", sys_info.version);
    printf(" Release = %s\n", sys_info.release);
    printf(" Architecture = %s\n", sys_info.machine);
    printf(" System running since last reboot: %s\n", uptime_str);
    printf("---------------------------------------\n");

}
void display_system_info_system(const ProgramConfig *config, char *cpu_info_buffer,char *memory_info_buffer) {
    printf("\033[H\033[J");
    char memory_lines[config->sample_count][128]; // Array to store memory info strings
    FILE *fp;
    char line[128];
    long memory_usage = 0;
    char *endptr;

    fp = fopen("/proc/self/status", "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                memory_usage = strtol(line + 6, &endptr, 10);
                // Check for conversion errors
                if (*endptr != '\0' && *endptr != ' ' && *endptr != '\n') {
                    fprintf(stderr, "Error parsing memory usage: %s", line);
                    fclose(fp);
                    return;
                }
                break;
            }
        }
        fclose(fp);
    }
    struct utsname sys_info;
    uname(&sys_info);
    char uptime_str[100];
    read_uptime(uptime_str, sizeof(uptime_str));
    // Print static information first
    printf("Nbr of samples: %d -- every %d secs\n", config->sample_count, config->time_delay);
    printf("Memory usage of this tool: %ld kB\n", memory_usage);
    printf("---------------------------------------\n");
    // Print static information first
    printf("### Memory ### (Phys.Used/Tot -- Virtual Used/Tot)\n");
    for (int i = 0; i < config->sample_count; i++) {
        printf("\n"); // Print placeholder lines for memory information
    }
    printf("---------------------------------------\n");
    printf("Number of cores: %ld\n", sysconf(_SC_NPROCESSORS_ONLN));
    double cpu_usage_1;
    //char cpu_info_buffer[256];
    gather_cpu_info(cpu_info_buffer, sizeof(cpu_info_buffer),config->time_delay,&cpu_usage_1);
    printf(" total cpu use = %.2f%%\n", cpu_usage_1);
    printf("---------------------------------------\n");
    // Move the cursor up to the start of the memory information section
    printf("\033[%dA", config->sample_count + 4); //change this later
    // Update only the memory information section in each loop
    for (int i = 0; i < config->sample_count; i++) {
        double cpu_usage;
        long phys_used_mem, total_phys_mem, virt_used_mem, total_virt_mem;
        //char memory_info_buffer[256];
        gather_cpu_info(cpu_info_buffer, sizeof(cpu_info_buffer),config->time_delay,&cpu_usage);
        gather_memory_info(&phys_used_mem, &total_phys_mem, &virt_used_mem, &total_virt_mem, memory_info_buffer, sizeof(memory_info_buffer));
        snprintf(memory_lines[i % config->sample_count], sizeof(memory_lines[0]),
                 "%.2f GB / %.2f GB -- %.2f GB / %.2f GB",
                 (double)phys_used_mem / 1024.0 / 1024, (double)total_phys_mem / 1024.0 / 1024,
                 ((double)virt_used_mem / 1024.0 / 1024)+(double)phys_used_mem / 1024.0 / 1024, (double)total_virt_mem / 1024.0 / 1024+(double)total_phys_mem / 1024.0 / 1024);

        // Clear and reprint memory lines
        //sleep((config->time_delay));
        for (int j = 0; j < config->sample_count; j++) {
            printf("\033[K"); // Clear the line
            if (j <= i) {
                printf("%s\n", memory_lines[j]);
            } else {
                printf("\n");
            }
        }
        gather_cpu_info(cpu_info_buffer, sizeof(cpu_info_buffer),config->time_delay,&cpu_usage);
        printf("\033[%dB",2);
        printf("\033[2K");
        printf("\rtotal cpu use = %.2f%%", cpu_usage);
        printf("\033[%dA",2);
        printf("\033[%dD",21);

        // Position cursor for next update
        if (i < config->sample_count-1) {
            printf("\033[%dA", config->sample_count);
            sleep((config->time_delay));
        }
        else {
            printf("\033[%dB", 4);
        }
        //sleep((config->time_delay*0.85));
    }
    printf("---------------------------------------\n");
    printf("### System Information ###\n");
    read_uptime(uptime_str, sizeof(uptime_str));
    printf(" System Name = %s\n", sys_info.sysname);
    printf(" Machine Name = %s\n", sys_info.nodename);
    printf(" Version = %s\n", sys_info.version);
    printf(" Release = %s\n", sys_info.release);
    printf(" Architecture = %s\n", sys_info.machine);
    printf(" System running since last reboot: %s\n", uptime_str);
    printf("---------------------------------------\n");
}

void display_system_info_sequential(const ProgramConfig *config, char *cpu_info_buffer,char *memory_info_buffer,char *users_info_buffer) {
    // Initial setup and static information
    printf("\033[H\033[J");
    char memory_lines[config->sample_count][128]; // Array to store memory info strings
    FILE *fp;
    char line[128];
    long memory_usage = 0;
    char *endptr;

    fp = fopen("/proc/self/status", "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                memory_usage = strtol(line + 6, &endptr, 10);
                // Check for conversion errors
                if (*endptr != '\0' && *endptr != ' ' && *endptr != '\n') {
                    fprintf(stderr, "Error parsing memory usage: %s", line);
                    fclose(fp);
                    return;
                }
                break;
            }
        }
        fclose(fp);
    }

    printf("Memory usage of this tool: %ld kB\n", memory_usage);
    struct utsname sys_info;
    uname(&sys_info);
    char uptime_str[100];
    read_uptime(uptime_str, sizeof(uptime_str));

    // Static system information

    for (int iteration = 0; iteration < config->sample_count; iteration++) {
        printf(">>>> iteration: %d\n", iteration);
        printf("Nbr of samples: %d -- every %d secs\n", config->sample_count, config->time_delay);
        if (memory_usage > 0) {
            printf(" Memory usage: %ld kB\n", memory_usage);
        } else {
            printf(" Memory usage of: Not available\n");
        }
        printf("---------------------------------------\n");
        double cpu_usage;
        long phys_used_mem, total_phys_mem, virt_used_mem, total_virt_mem;
        //char cpu_info_buffer[256]; // Buffer to hold CPU info
        //gather_cpu_info(cpu_info_buffer, sizeof(cpu_info_buffer),config->time_delay);
        //gather_memory_info(&phys_used_mem, &total_phys_mem, &virt_used_mem, &total_virt_mem);
        //char memory_info_buffer[256];
        gather_memory_info(&phys_used_mem, &total_phys_mem, &virt_used_mem, &total_virt_mem, memory_info_buffer, sizeof(memory_info_buffer));
        printf("### Memory ### (Phys.Used/Tot -- Virtual Used/Tot)\n");
        for (int i = 0; i < config->sample_count; i++) {
            printf("\n"); // Print placeholder lines for memory information
        }
        printf("\033[%dA",config->sample_count-iteration);
        printf("\033[2k");
        printf( "\r%.2f GB / %.2f GB -- %.2f GB / %.2f GB",
                (double)phys_used_mem / 1024.0 / 1024, (double)total_phys_mem / 1024.0 / 1024,
                ((double)virt_used_mem / 1024.0 / 1024)+(double)phys_used_mem / 1024.0 / 1024, (double)total_virt_mem / 1024.0 / 1024+(double)total_phys_mem / 1024.0 / 1024);
        printf("\033[%dB",config->sample_count-iteration);
        printf("\033[%dD",41);
        // User sessions
        printf("---------------------------------------\n");
        printf("### Sessions/users ###\n");
        int user_session_count = 0;
        //list_user_sessions(&user_session_count);
        //char users_info_buffer[4096];
        list_user_sessions(&user_session_count, users_info_buffer, sizeof(users_info_buffer));
        // CPU usage
        printf("---------------------------------------\n");
        printf("Number of cores: %ld\n", sysconf(_SC_NPROCESSORS_ONLN));
        //printf(" total cpu use = %s\n", cpu_info_buffer);
        gather_cpu_info(cpu_info_buffer, sizeof(cpu_info_buffer),config->time_delay,&cpu_usage);
        printf(" total cpu use = %.2f%%\n", cpu_usage);
        // Delay for the specified time if needed 1234567890987654
        if (iteration < config->sample_count) {
          sleep(config->time_delay);
        }
    }
    printf("---------------------------------------\n");
    printf("### System Information ###\n");
    printf(" System Name = %s\n", sys_info.sysname);
    printf(" Machine Name = %s\n", sys_info.nodename);
    printf(" Version = %s\n", sys_info.version);
    printf(" Release = %s\n", sys_info.release);
    printf(" Architecture = %s\n", sys_info.machine);
    printf(" System running since last reboot: %s\n", uptime_str);
    printf("---------------------------------------\n");
}

void display_user_info(const ProgramConfig *config,char *users_info_buffer) {
    printf("\033[H\033[J");
    FILE *fp;
    char line[128];
    long memory_usage = 0;
    char *endptr;

    fp = fopen("/proc/self/status", "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                memory_usage = strtol(line + 6, &endptr, 10);
                // Check for conversion errors
                if (*endptr != '\0' && *endptr != ' ' && *endptr != '\n') {
                    fprintf(stderr, "Error parsing memory usage: %s", line);
                    fclose(fp);
                    return;
                }
                break;
            }
        }
        fclose(fp);
    }
    struct utsname sys_info;
    uname(&sys_info);
    char uptime_str[100];
    read_uptime(uptime_str, sizeof(uptime_str));
    // Print static information first
    printf("Nbr of samples: %d -- every %d secs\n", config->sample_count, config->time_delay);
    printf(" Memory usage of this tool: %ld kB\n", memory_usage);
    printf("---------------------------------------\n");
    printf("### Sessions/users ###\n");
    int user_session_count = 0;
    list_user(&user_session_count);
    //list_user_sessions(&user_session_count);
    //printf("# user = %d\n",user_session_count);
    for (int i = 0; i < user_session_count; i++) {
        printf("\n"); // Print placeholder lines for memory information
    }
    printf("---------------------------------------\n");
    printf("\033[%dA", user_session_count+1);
    sleep(config->time_delay);
    //char users_info_buffer[4096];
    list_user_sessions(&user_session_count, users_info_buffer, sizeof(users_info_buffer));
    //list_user_sessions(&user_session_count);
    printf("\033[%dB", 1);
    printf("---------------------------------------\n");
    printf("### System Information ###\n");
    read_uptime(uptime_str, sizeof(uptime_str));
    printf(" System Name = %s\n", sys_info.sysname);
    printf(" Machine Name = %s\n", sys_info.nodename);
    printf(" Version = %s\n", sys_info.version);
    printf(" Release = %s\n", sys_info.release);
    printf(" Architecture = %s\n", sys_info.machine);
    printf(" System running since last reboot: %s\n", uptime_str);
    printf("---------------------------------------\n");
    sleep(1);
}

typedef struct {
    int current_index;
} UsageHistory;

UsageHistory usage_history;

void initialize_usage_history() {
    memset(&usage_history, 0, sizeof(usage_history));
}

void graphical_system_info(const ProgramConfig *config, char *cpu_info_buffer,char *memory_info_buffer,char *users_info_buffer) {
    printf("\033[H\033[J"); // Clear screen
    char memory_lines[config->sample_count][128]; // Adjust the array size based on sample_count
    char cpu_lines[config->sample_count][128];    // Array for CPU usage representation
    FILE *fp;
    char line[128];
    long memory_usage = 0;
    char *endptr;

    fp = fopen("/proc/self/status", "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                memory_usage = strtol(line + 6, &endptr, 10);
                // Handle conversion errors
                if (*endptr != '\0' && *endptr != ' ' && *endptr != '\n') {
                    fprintf(stderr, "Error parsing memory usage: %s", line);
                    fclose(fp);
                    printf("Memory usage of this tool: Not available\n");
                    return;
                }
                break;
            }
        }
        fclose(fp);
    }
    struct utsname sys_info;
    uname(&sys_info);
    char uptime_str[100];
    read_uptime(uptime_str, sizeof(uptime_str));
    printf("Nbr of samples: %d -- every %d secs\n", config->sample_count, config->time_delay);
    printf("Memory usage of this tool: %ld kB\n", memory_usage);
    printf("---------------------------------------\n");
    printf("### Memory ### (Phys.Used/Tot -- Virtual Used/Tot)\n");
    // Print placeholders for memory and CPU information
    for (int i = 0; i < config->sample_count; i++) {
        printf("\n");  // Memory placeholder
    }
    printf("---------------------------------------\n");
    printf("### Sessions/users ###\n");
    int user_session_count = 0;
    //char users_info_buffer[4096];
    list_user_sessions(&user_session_count, users_info_buffer, sizeof(users_info_buffer));
    //list_user_sessions(&user_session_count);
    printf("---------------------------------------\n");
    printf("Number of cores: %ld\n", sysconf(_SC_NPROCESSORS_ONLN));
    double cpu_usage_3;
    //char cpu_info_buffer[256]; // Buffer to hold CPU info
    //gather_cpu_info(cpu_info_buffer, sizeof(cpu_info_buffer), config->time_delay);
    gather_cpu_info(cpu_info_buffer, sizeof(cpu_info_buffer),config->time_delay, &cpu_usage_3);
    printf("total cpu use = %.2f%%\n", cpu_usage_3);
    //printf("total cpu use = %s\n", cpu_info_buffer);
    // Placeholder for CPU information
    for (int i = 0; i < config->sample_count; i++) {
        printf("\n");  // CPU placeholder
    }
    printf("\033[%dA", config->sample_count+config->sample_count+user_session_count+5);
    // Loop for Memory Graphics
    double prev_memory_usage = 0;
    double prev_cpu_usage = 0;
    for (int i = 0; i < config->sample_count; i++) {
        char cpu_info_buffer_1[256]; // Buffer to hold CPU info
        //gather_cpu_info(cpu_info_buffer_1, sizeof(cpu_info_buffer_1), config->time_delay);
        double cpu_usage;
        gather_cpu_info(cpu_info_buffer, sizeof(cpu_info_buffer),config->time_delay, &cpu_usage);
        long phys_used_mem, total_phys_mem, virt_used_mem, total_virt_mem;
        //gather_cpu_info(&cpu_usage, sizeof(cpu_info_buffer_1),config->time_delay);
        //gather_memory_info(&phys_used_mem, &total_phys_mem, &virt_used_mem, &total_virt_mem);
        //char memory_info_buffer[256];
        gather_memory_info(&phys_used_mem, &total_phys_mem, &virt_used_mem, &total_virt_mem, memory_info_buffer, sizeof(memory_info_buffer));

        double current_memory_usage = (double)phys_used_mem / 1024.0 / 1024; // Convert to GB
        double memory_change = current_memory_usage - prev_memory_usage;

        prev_memory_usage = current_memory_usage;
        if (i==0){
            memory_change = 0;
        }
        char memory_change_graphic[50] = "|"; // Start with the separator symbol
        int change_magnitude = (int)(fabs(memory_change) * 10); // Scale change to control number of symbols
        if (memory_change >= 0) {
            memset(memory_change_graphic + 1, '#', change_magnitude);
            memory_change_graphic[change_magnitude + 1] = '\0'; // Null-terminate the string
            snprintf(memory_change_graphic + change_magnitude + 1, sizeof(memory_change_graphic) - change_magnitude - 1, "* %.2f (%.2f)", memory_change, current_memory_usage);
        } else if (memory_change < 0) {
            memset(memory_change_graphic + 1, ':', change_magnitude);
            memory_change_graphic[change_magnitude + 1] = '\0'; // Null-terminate the string
            snprintf(memory_change_graphic + change_magnitude + 1, sizeof(memory_change_graphic) - change_magnitude - 1, "@ %.2f (%.2f)", fabs(memory_change), current_memory_usage);
        }
        snprintf(memory_lines[i], sizeof(memory_lines[0]), "%.2f GB / %.2f GB -- %.2f GB / %.2f GB   %s",
                 current_memory_usage, (double)total_phys_mem / 1024.0 / 1024,
                 ((double)virt_used_mem / 1024.0 / 1024)+(double)phys_used_mem / 1024.0 / 1024, (double)total_virt_mem / 1024.0 / 1024+(double)total_phys_mem / 1024.0 / 1024,
                 memory_change_graphic);
        printf("\033[K%s\n", memory_lines[i]); // Clear and print memory line
        if (i < config->sample_count) {
            printf("\033[%dB", config->sample_count + user_session_count +4);
        }
        sleep(config->time_delay);
        //gather_cpu_info(&cpu_usage, sizeof(cpu_info_buffer),config->time_delay);
        //gather_cpu_info(cpu_info_buffer_1, sizeof(cpu_info_buffer_1), config->time_delay);
        //gather_memory_info(&phys_used_mem, &total_phys_mem, &virt_used_mem, &total_virt_mem);
        char cpu_change_graphic[50] = "        "; // Start with the separator symbol
        strcat(cpu_change_graphic, "||"); // Start with the separator symbol

        // Extract the CPU usage value from the buffer
        //double cpu_usage_value;
        //sscanf(cpu_info_buffer, "\r total cpu use =: %lf%%", &cpu_usage_value);
        int change_magnitude_cpu = (int)(cpu_usage * 40); // Scale change to control number of symbols


// Fill the graphic representation based on change magnitude
        for (int k = 0; k < change_magnitude_cpu; k++) {
            strcat(cpu_change_graphic, "|");
        }

// Append the numeric change to the graphic
        char change_buffer[20];
        snprintf(change_buffer, sizeof(change_buffer), " %.2f",cpu_usage);
        strcat(cpu_change_graphic, change_buffer);
        // Update CPU line
        snprintf(cpu_lines[i], sizeof(cpu_lines[0]), "%s", cpu_change_graphic);
        printf("\033[K%s\n", cpu_lines[i]); // Clear and print CPU line
        printf("\033[%dA",i+2);
        printf("\033[2K");
        printf("\rtotal cpu use = %.2f%%", cpu_usage);
        //printf("\rtotal cpu use = %s", cpu_info_buffer);
        //gather_cpu_info(cpu_info_buffer, sizeof(cpu_info_buffer),config->time_delay);
        printf("\033[%dB",i+2);
        printf("\033[%dD",21);

        // Move cursor back up for the next memory line (if not the last iteration)
        if (i < config->sample_count-1 ) {
            printf("\033[%dA", config->sample_count + 5 + user_session_count);
        }
        sleep(config->time_delay);
    }
    // Print static system information
    printf("---------------------------------------\n");
    printf("### System Information ###\n");
    printf(" System Name = %s\n", sys_info.sysname);
    printf(" Machine Name = %s\n", sys_info.nodename);
    printf(" Version = %s\n", sys_info.version);
    printf(" Release = %s\n", sys_info.release);
    printf(" Architecture = %s\n", sys_info.machine);
    printf(" System running since last reboot: %s\n", uptime_str);
    printf("---------------------------------------\n");
}
