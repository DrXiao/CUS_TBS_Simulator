#ifndef __TASK_H__
#define __TASK_H__
#include "list.h"
#define DEBUG 1

extern int clock;

struct periodic_task{
    int period;
    int WCET;
    // double utilization;
};

struct aperiodic_task{
    int arrival_time;
    int WCET;
};

struct job {
    int release_time;
    int remain_exec_time;
    int abs_deadlne;
    int tid;
};

struct job_queue_node {
    struct job job;
    struct list_head link;
};

int init_tasks_info(char *, char *, char *);

void terminate_system(void);

void check_jobs_miss_deadline(void);

void check_new_jobs_release(void);

void execute_job(void);

void job_queue_add_tail(struct list_head *, struct job *);

void destroy_job_queue(struct list_head *);

int edf_cmp_jobs(struct job *, struct job *) ;

#endif
