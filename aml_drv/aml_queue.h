#ifndef _AML_QUEUE_H_
#define _AML_QUEUE_H_

#include <linux/interrupt.h>

#define AML_MISC_Q_NUM 20
#define INC_QUEUE_INDX(indx, max_num) ((((indx) + 1) < (max_num)) ? ((indx) + 1) : (0))

struct msg_q_t {
    unsigned int wt;
    unsigned int rd;
    unsigned int max;
    unsigned int size;
    spinlock_t misc_lock;
    void *q_buf;
};

struct misc_msg_t {
    unsigned int id;
    void* env;
};

enum {
    MISC_EVENT_SYNC_TRACE = 0,
    MISC_EVENT_BCN_UPDATE,
};

extern struct msg_q_t aml_misc_q;
extern struct misc_msg_t misc_msg[AML_MISC_Q_NUM];

extern int aml_misc_queue_init(struct msg_q_t *q);
extern int aml_misc_queue_deinit(struct msg_q_t *q);
extern int aml_enqueue(struct msg_q_t *q, void *msg);
extern int aml_dequeue(struct msg_q_t *q, void *msg);

#endif /* _AML_QUEUE_H_ */

