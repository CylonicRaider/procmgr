/* procmgr -- init-like process manager
 * https://github.com/CylonicRaider/procmgr */

/* Job queue management
 * Each job contains a callback and a data pointer for the callback, an
 * optional PID which delays the job until the process specified dies, and an
 * optional "successor" job, which is scheduled to wait on the process of
 * this job once it is started. Successor jobs are queued before any other
 * jobs that could reference the same PID (i.e., in the very beginning of the
 * queue). */

#ifndef _JOBS_H
#define _JOBS_H

/* Not caused by process exit */
#define JOB_NOEXIT 65535

/* Callback type for jobs
 * The first argument is passed through from the data member of the job
 * structure. The integer argument is the exit code of the process that
 * caused the callback to be invoked, or the JOB_NOEXIT constant if the
 * execution was caused by other means. The callback is expected to return
 * the PID of a process spawned, 0 when no process was spawned, or -1 on
 * error (with errno set). */
typedef int job_func_t(void *, int);

/* Destructor for auxillary job data
 * The function is called when the job is destroyed. The pointer is the
 * auxillary data pointer as obtained from the job structure.
 * NOTE that the prototype is compatible to free(), which might be usable
 *      if only the data itself are dynamically allocated. */
typedef void job_destr_t(void *);

/* A single job
 * Members:
 * callback  : (job_func_t *) The function to run in this job. May be NULL,
 *             although such a job is rather pointless.
 * destroy   : (job_destr_t *) Deallocate the data pointer for this job.
 *             May be NULL (but should not be in most cases).
 * data      : (void *) The data to pass to the callback.
 * waitfor   : (int) The PID of a process this job is waiting for.
 * notBefore : (double) The UNIX timestamp before which to ignore this job.
 *             Used to implement delays.
 * successor : (struct job *) A job to spawn once this one is finished.
 * prev, next: (struct job *) Linked list interconnection. */
struct job {
    job_func_t *callback;
    job_destr_t *destroy;
    void *data;
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
struct job *job_new(job_func_t *callback, job_destr_t *destroy, void *data);

/* Deinitialize this job and deallocate all of its successors
 * For cleaning up the auxillary data, the configured destructor is invoked;
 * if none is configured, the data are just discarded, possibly causing a
 * memory leak.
 * Linked list neigbors are changed to be connected with each other. */
void job_del(struct job *job);

/* Deinitialize and deallocate this job
 * See also the remarks for job_del(). */
void job_free(struct job *job);

/* Actually run the callback associated with the job
 * retcode is passed through to the callback; use JOB_NOEXIT if you have no
 * meaningful value to specify.
 * The return value is the same as the one of the callback (in particular,
 * -1 on error), or zero if no callback is configured. */
int job_run(struct job *job, int retcode);

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
