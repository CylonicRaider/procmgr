/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

#include <stdlib.h>

#include "jobs.h"

/* Create a new empty job queue */
struct jobqueue *jobqueue_new() {
    return calloc(1, sizeof(struct jobqueue));
}

/* Deinitialize this job queue and deallocate all jobs in it */
void jobqueue_del(struct jobqueue *queue) {
    if (queue->head) job_free(queue->head);
    queue->head = NULL;
    queue->tail = NULL;
}

/* Deinitialize and deallocate this job queue */
void jobqueue_free(struct jobqueue *queue) {
    jobqueue_del(queue);
    free(queue);
}

/* Create a new job with the given parameters */
struct job *job_new(char *filename, char **argv, char **envp) {
    struct job *ret = calloc(1, sizeof(struct job));
    if (! ret) return NULL;
    ret->filename = filename;
    ret->argv = argv;
    ret->envp = envp;
    ret->waitfor = -1;
    return ret;
}

/* Deinitialize this job and deallocate all of its successors */
void job_del(struct job *job) {
    char **p;
    if (job->argv) for (p = job->argv; *p; p++) free(*p);
    if (job->envp) for (p = job->envp; *p; p++) free(*p);
    free(job->filename);
    free(job->argv);
    free(job->envp);
    job->filename = NULL;
    job->argv = NULL;
    job->envp = NULL;
    job->waitfor = -1;
    if (job->successor) job_free(job->successor);
    job->successor = NULL;
    if (job->prev) job->prev->next = job->next;
    if (job->next) job->next->prev = job->prev;
    job->prev = NULL;
    job->next = NULL;
}

/* Deinitialize and deallocate this job */
void job_free(struct job *job) {
    struct job *prev, *next, *cur;
    for (cur = job->prev; cur; cur = prev) {
        prev = cur->prev;
        job_del(cur);
        free(cur);
    }
    for (cur = job->next; cur; cur = next) {
        next = cur->next;
        job_del(next);
        free(next);
    }
    job_del(job);
    free(job);
}

/* Prepend a job to the queue */
void jobqueue_prepend(struct jobqueue *queue, struct job *job) {
    if (! queue->head) {
        queue->head = job;
        queue->tail = job;
        job->next = NULL;
    } else {
        job->next = queue->head;
        queue->head = job;
    }
    job->prev = NULL;
    if (job->next) job->next->prev = job;
}

/* Append a job to the queue */
void jobqueue_append(struct jobqueue *queue, struct job *job) {
    if (! queue->tail) {
        queue->head = job;
        queue->tail = job;
        job->prev = NULL;
    } else {
        job->prev = queue->tail;
        queue->tail = job;
    }
    job->next = NULL;
    if (job->prev) job->prev->next = job;
}

/* Extract the given job from the queue, and return it */
struct job *jobqueue_take(struct jobqueue *queue, struct job *job) {
    if (queue->head == job) queue->head = job->next;
    if (queue->tail == job) queue->tail = job->prev;
    if (queue->head) queue->head->prev = NULL;
    if (queue->tail) queue->tail->next = NULL;
    job->prev = NULL;
    job->next = NULL;
    return job;
}

/* Get all the jobs matching the given PID from the queue */
struct job *jobqueue_getfor(struct jobqueue *queue, int pid) {
    struct job *ret = NULL, *cur, *prev;
    for (cur = queue->tail; cur; cur = prev) {
        prev = cur->prev;
        if (cur->waitfor == pid) {
            jobqueue_take(queue, cur);
            if (ret) ret->prev = cur;
            cur->next = ret;
            ret = cur;
        }
    }
    return ret;
}
