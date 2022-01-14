#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include "task.h"
#include "list.h"

// periodic tasks settings.
static struct periodic_task *periodic_tasks = NULL;
static struct list_head periodic_job_queue = INIT_LIST_HEAD(periodic_job_queue);
static int miss_periodic_job_num = 0;
static int total_periodic_jobs_num = 0;
static int num_of_periodic_tasks = 0;

// aperiodic tasks settings.
static struct aperiodic_task *aperiodic_tasks = NULL;
static struct list_head aperiodic_job_queue =
    INIT_LIST_HEAD(aperiodic_job_queue);
static int total_response_time = 0;
static int total_completed_aperiodic_job_num = 0;
static int num_of_aperiodic_tasks = 0;
static int arrived_ap_task_idx = 0;

// server settings.
static int server_deadline = 0;
static int server_budget = 0;
static double server_size = 0.2;
static char *server_type = NULL;
static bool CUS = false;
static bool TBS = false;

// system settings.
static bool has_new_job = false;
static struct job_queue_node *exec_job = NULL;
static bool doing_aperiodic = false;
int clock = 0;

int init_tasks_info(char *server, char *periodic_file, char *aperiodic_file) {
    CUS = !strcmp(server, "CUS");
    TBS = !strcmp(server, "TBS");
    FILE *periodic_input = fopen(periodic_file, "r");
    if (periodic_input == NULL) {
        printf("File %s Not Found\n", periodic_file);
        exit(0);
    }
    miss_periodic_job_num = 0;
    total_periodic_jobs_num = 0;
    num_of_periodic_tasks = 0;
    struct periodic_task p_task;
    while (fscanf(periodic_input, "%d, %d", &p_task.period, &p_task.WCET) !=
           EOF) {
        periodic_tasks = realloc(periodic_tasks, sizeof(struct periodic_task) *
                                                     ++num_of_periodic_tasks);
        periodic_tasks[num_of_periodic_tasks - 1] = p_task;
    }
    fclose(periodic_input);

    FILE *aperiodic_input = fopen(aperiodic_file, "r");
    if (aperiodic_file == NULL) {
        printf("File %s Not Found\n", aperiodic_file);
        exit(0);
    }
    total_response_time = 0;
    total_completed_aperiodic_job_num = 0;
    num_of_aperiodic_tasks = 0;
    struct aperiodic_task ap_task;
    while (fscanf(aperiodic_input, "%d, %d", &ap_task.arrival_time,
                  &ap_task.WCET) != EOF) {
        aperiodic_tasks =
            realloc(aperiodic_tasks,
                    sizeof(struct aperiodic_task) * ++num_of_aperiodic_tasks);
        aperiodic_tasks[num_of_aperiodic_tasks - 1] = ap_task;
    }
    fclose(aperiodic_input);
    printf("Tatal ap jobs: %d\n", num_of_aperiodic_tasks);
    return 0;
}

void terminate_system(void) {
#if DEBUG == 1
    struct list_head *cur;
    printf("Remaining aperiodic jobs: ");
    LIST_FOR_EACH(cur, &aperiodic_job_queue) {
        struct job_queue_node *accesser = (struct job_queue_node *)(CONTAINER_OF(
            cur, struct job_queue_node, link));
        printf("TA%d ->", accesser->job.tid);
    }
    printf("NULL\n");
#endif
    destroy_job_queue(&periodic_job_queue);
    destroy_job_queue(&aperiodic_job_queue);
    free(periodic_tasks);
    free(aperiodic_tasks);
#if DEBUG == 1
    printf("miss periodic jobs: %d, total periodic jobs: %d\n"
           "total response time: %d, number of completed aperiodic tasks: %d\n",
           miss_periodic_job_num, total_periodic_jobs_num, total_response_time,
           total_completed_aperiodic_job_num);

#endif
    printf("Miss rate of periodic jobs: %lf\n"
           "Average response time of aperiodic jobs: %lf\n",
           (double)miss_periodic_job_num / total_periodic_jobs_num,
           (double)total_response_time / total_completed_aperiodic_job_num);
}

void check_jobs_miss_deadline(void) {
    struct list_head *cur = NULL;
    struct job_queue_node *accesser = NULL;
    struct list_head *job_queue = &periodic_job_queue;
    for (cur = job_queue->next; cur != job_queue; cur = cur->next) {
        accesser = (struct job_queue_node *)(CONTAINER_OF(
            cur, struct job_queue_node, link));
        int abs_deadline = accesser->job.abs_deadlne;
        int remain_exec_time = accesser->job.remain_exec_time;
        if (abs_deadline - clock - remain_exec_time < 0) {
            fprintf(stderr,
                    "At clock %d: Job %d misses deadline (abs_deadline: %d, "
                    "remain_exec_time: %d)\n",
                    clock, accesser->job.tid, accesser->job.abs_deadlne,
                    accesser->job.remain_exec_time);
            miss_periodic_job_num += 1;
            cur = cur->next;
            list_del(&accesser->link);
            free(accesser);
            cur = cur->prev;
        }
    }
}

void check_new_jobs_release(void) {
    has_new_job = false;
    struct list_head *job_queue = &periodic_job_queue;
    for (int task_idx = 0; task_idx < num_of_periodic_tasks; task_idx++) {
        if ((clock) % periodic_tasks[task_idx].period == 0) {
            struct job new_job = (struct job){
                .abs_deadlne = clock + periodic_tasks[task_idx].period,
                .release_time = clock,
                .remain_exec_time = periodic_tasks[task_idx].WCET,
                .tid = task_idx + 1};
            job_queue_add_tail(job_queue, &new_job);
            has_new_job = true;
            total_periodic_jobs_num += 1;
        }
    }
    job_queue = &aperiodic_job_queue;

    while (arrived_ap_task_idx < num_of_aperiodic_tasks &&
           clock == aperiodic_tasks[arrived_ap_task_idx].arrival_time) {
        struct job new_job = (struct job){
            .abs_deadlne = -1,
            .release_time = clock,
            .remain_exec_time = aperiodic_tasks[arrived_ap_task_idx].WCET,
            .tid = arrived_ap_task_idx + 1};

        if (job_queue == job_queue->next) {
            if (CUS && clock >= server_deadline) {
                server_deadline = clock + new_job.remain_exec_time * 5;
                server_budget = new_job.remain_exec_time;
            }
            if (TBS) {
                server_deadline =
                    (server_deadline >= clock ? server_deadline : clock) +
                    new_job.remain_exec_time * 5;
                server_budget = new_job.remain_exec_time;
            }
        }
        
        job_queue_add_tail(job_queue, &new_job);
        has_new_job = true;
        arrived_ap_task_idx += 1;
    }
}

void execute_job(void) {
    struct list_head *job_queue = &periodic_job_queue;
    struct list_head *cur = NULL;
    struct job_queue_node *accesser = NULL;
    int aperiodic_job_release_time = 0;

    if (has_new_job || !exec_job) {
        LIST_FOR_EACH(cur, job_queue) {
            accesser = (struct job_queue_node *)(CONTAINER_OF(
                cur, struct job_queue_node, link));
            if (!exec_job ||
                (!doing_aperiodic && edf_cmp_jobs(&exec_job->job, &accesser->job) >= 0) ||
                (doing_aperiodic && server_deadline >= accesser->job.abs_deadlne)) {
                exec_job = accesser;
                doing_aperiodic = false;
            }
        }
        job_queue = &aperiodic_job_queue;
        cur = job_queue->next;
        
        if (cur != job_queue) {
            accesser = (struct job_queue_node *)(CONTAINER_OF(
                cur, struct job_queue_node, link));
            if (server_budget &&
                (!exec_job || exec_job->job.abs_deadlne >= server_deadline)) {
                exec_job = accesser;
                doing_aperiodic = true;
            }
        }
    }
#if DEBUG == 1
    if (exec_job)
        printf("clock %d: %s%d, server deadline:%d\n", clock, (doing_aperiodic ? "TA" : "T"),
               exec_job->job.tid, server_deadline);
    else
        printf("clock %d: %p\n", clock, NULL);
#endif
    if (exec_job) {

        exec_job->job.remain_exec_time -= 1;
        if (doing_aperiodic) {
            server_budget -= 1;
        }
        if (exec_job->job.remain_exec_time == 0) {
            if (doing_aperiodic) {
                total_completed_aperiodic_job_num += 1;
                aperiodic_job_release_time = exec_job->job.release_time;
                total_response_time += 1 + clock - aperiodic_job_release_time;
#if DEBUG == 1
                printf("TA%d finishs: %d ~ %d\n", exec_job->job.tid ,aperiodic_job_release_time, 1 + clock);
                printf("Current total_response_time: %d\n", total_response_time);        
#endif
            }
            list_del(&exec_job->link);
            free(exec_job);
            exec_job = NULL;
            if (doing_aperiodic && TBS) {
                goto SERVER_SETTING;
            }
        }
    }
    if (CUS && server_deadline == clock + 1) {
    SERVER_SETTING:
        job_queue = &aperiodic_job_queue;
        cur = job_queue->next;
        if (cur != job_queue) {
            accesser = (struct job_queue_node *)(CONTAINER_OF(
                cur, struct job_queue_node, link));
            printf("For TA%d, New deadline: %d + %d ", accesser->job.tid, server_deadline, accesser->job.remain_exec_time * 5);
            server_deadline += accesser->job.remain_exec_time * 5;
            server_budget = accesser->job.remain_exec_time;
            printf("= %d\n", server_deadline);
        }
    }
}

void job_queue_add_tail(struct list_head *job_queue, struct job *job) {
    struct list_head *cur = NULL;
    for (cur = job_queue; cur->next != job_queue; cur = cur->next)
        ;
    struct job_queue_node *new_job = calloc(1, sizeof(struct job_queue_node));
    new_job->job = *job;
    list_add_tail(&new_job->link, cur);
}

void destroy_job_queue(struct list_head *job_queue) {
    struct list_head *cur = NULL;
    for (cur = job_queue->next; cur != job_queue; cur = job_queue->next) {
        struct job_queue_node *del_ptr = (struct job_queue_node *)(CONTAINER_OF(
            cur, struct job_queue_node, link));
        list_del(cur);
        free(del_ptr);
    }
}

int edf_cmp_jobs(struct job *job1, struct job *job2) {
    if (!job1) {
        return 1;
    }
    if (!job2) {
        return -1;
    }
    if (job1->abs_deadlne < job2->abs_deadlne)
        return -1;
    else if (job1->abs_deadlne == job2->abs_deadlne)
        return 0;
    else
        return 1;
}
