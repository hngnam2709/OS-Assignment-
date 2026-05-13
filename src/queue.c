#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
        if (q == NULL)
                return 1;
        return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: put a new process to queue [q] */
        if (q == NULL || proc == NULL)
                return;
        if (q->size >= MAX_QUEUE_SIZE)
                return;
        q->proc[q->size] = proc;
        q->size++;
}

struct pcb_t *dequeue(struct queue_t *q)
{
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
        if (q == NULL || empty(q))
		return NULL;
        int i, min_prio = MAX_PRIO, idx = -1;
        for (i = 0; i < q->size; i++){
                if (q->proc[i]->prio < min_prio){
                        min_prio = q->proc[i]->prio;
                        idx = i;
                }
        }
        if (idx == -1)
                return NULL;
        struct pcb_t *proc = q->proc[idx];
        for (i = idx; i < q->size - 1; i++){
                q->proc[i] = q->proc[i + 1];
        }
        q->size--;
        return proc;
}

struct pcb_t *purgequeue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: remove a specific item from queue
         * */
        if (q == NULL || proc == NULL || empty(q)){
            return NULL;    
        }
        int i, idx = -1;
        for (i = 0; i < q->size; i++){
                if (q->proc[i] == proc){
                        idx = i;
                        break;
                }
        }
        if (idx == -1)
                return NULL;
        struct pcb_t *result = q->proc[idx];
        for (i = idx; i < q->size - 1; i++){
                q->proc[i] = q->proc[i + 1];
        }
        q->size--;
        return result;
}