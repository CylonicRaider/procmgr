/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Job queue management
 * Each job contains a set of parameters for an execve() call, an optional
 * PID which delays the job until the process specified dies, and an optional
 * "successor" job, which is scheduled to wait on the process of this job
 * once it is started. Successor jobs are queued before any other jobs that
 * could reference the same PID (i.e., in the very beginning of the queue).
 */

#ifndef _JOBS_H
#define _JOBS_H

/* Call-back type for jobs */
typedef int job_func_t(char *, char **, char **);

/* A single job
 * Members:
 * callback  : (job_func_t *) The function to run in this job. If NULL, a new
 *             process is forked, and execve() is used in there.
 * filename  : (char *) The file to spawn when this job is started.
 * argv      : (char **) Argument vector to pass to the program spawned.
 * envp      : (char **) Environment to pass to the program.
 * waitfor   : (int) The PID of a process this job is waiting for.
 * notBefore : (double) The UNIX timestamp before which to ignore this job.
 *             Used to implement delays.
 * successor : (struct job *) A job to spawn once this one is finished.
 * prev, next: (struct job *) Linked list interconnection. */
struct job {
    job_func_t *callback;
    char *filename;
    char **argv;
    char **envp;
    int waitfor;
    double notBefore;
    struct job *successor;
    struct job *prev, *next;
};

/* A job queue
 * Members:
 * head, tail: (struct job *) The first/last job in this queue. */
struct jobqueue {
    struct job *head;
    struct job *tail;
};

/* Create a new empty job queue */
struct jobqueue *jobqueue_new(void);

/* Deinitialize this job queue and deallocate all jobs in it */
void jobqueue_del(struct jobqueue *queue);

/* Deinitialize and deallocate this job queue */
void jobqueue_free(struct jobqueue *queue);

/* Create a new job with the given parameters */
struct job *job_new(char *filename, char **argv, char **envp);

/* Deinitialize this job and deallocate all of its successors
 * This assumes that all strings (and the arrays) are dynamically allocated.
 * Dispose of them manually if they are not.
 * Linked list neigbors are changed to be connected with each other. */
void job_del(struct job *job);

/* Deinitialize and deallocate this job
 * See also the remarks for job_del(). */
void job_free(struct job *job);

/* Prepend a job to the queue */
void jobqueue_prepend(struct jobqueue *queue, struct job *job);

/* Append a job to the queue */
void jobqueue_append(struct jobqueue *queue, struct job *job);

/* Extract the given job from the queue, and return it */
struct job *jobqueue_take(struct jobqueue *queue, struct job *job);

/* Get all the jobs matching the given PID from the queue
 * If a job's notBefore member is not NaN and before the current timestamp,
 * it is ignored.
 * Returns the head of a linked list of struct job-s, which are removed
 * from the queue itself; the return value may be NULL. */
struct job *jobqueue_getfor(struct jobqueue *queue, int pid);

#endif
