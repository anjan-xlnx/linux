#ifndef PCD_PF_DEV_H
#define PCD_PF_DEV_H

struct pcd_dev_info {
	int size;
	const char *serial_num;
	int perm;
};

enum ACC_PERM {
	RD_WR = 0,
	RD_ONLY,
	WR_ONLY,
};

#endif
