#define NPROC         64 // 最大进程数
#define KSTACKSIZE  4096 // 每个进程的内核栈大小
#define NCPU           8 // 最大 CPU 数量
#define NOFILE        16 // 每个进程的打开文件数量
#define NFILE        100 // 每个系统的打开文件数量
#define NINODE        50 // 最大活跃 i-node 数量
#define NDEV          10 // 最大主设备数量
#define ROOTDEV        1 //文件系统根磁盘的驱动数量
#define MAXARG        32 // exec 最大参数数量
#define MAXOPBLOCKS   10 // 文件系统每次最大的写操作 block 数
#define LOGSIZE       (MAXOPBLOCKS*3) // 磁盘日志中最大数据 block 数
#define NBUF          (MAXOPBLOCKS*#) // 磁盘 block 缓冲大小
#define FSSIZE        1000 // 在 block 中的文件系统大小
