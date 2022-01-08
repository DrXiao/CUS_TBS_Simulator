#include <stdio.h>
#include <stdlib.h>
#include "task.h"

int main(int argc, char **argv) {

    if (argc < 4) {
        printf("./main periodic.txt aperiodic.txt server_type\n");
        exit(0);
    }

    char *periodic_file = argv[1];
    char *aperiodic_file = argv[2];
    char *server_type = argv[3];
    
    clock = 0;
    
    int max_clock = 1000;
    
    printf("=== %s ===\n", server_type);
    init_tasks_info(server_type, periodic_file, aperiodic_file);
    
    while(clock <= max_clock) {
        check_jobs_miss_deadline();
        check_new_jobs_release();
        execute_job();   
        clock++;
    }
    
    terminate_system();
    
    

    return 0;
}