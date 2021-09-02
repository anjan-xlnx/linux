#ifndef SCULL_H
#define SCULL_H

#define SCULL_QUNATUM	4000
#define SCULL_QSET	1000
#define SCULL_DEV_CNT 4
#define SCULL_DEV_NAME "scull_dev"

struct scull_qset {
	void **data;
	struct scull_qset *next;
};

struct scull_dev {
	struct scull_qset *qset;
	struct cdev cdev;
	unsigned long size;
	int quantum, qset_cnt;
};
#endif
