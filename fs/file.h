struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type;
  int32 ref; // 引用数量
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  uint32 off;
};

// 内存中 inode 的副本
struct inode {
  uint32 dev;  // 设备号
  uint32 inum; // inode 号
  int32 ref;   // 引用计数
  struct sleeplock lock; // 保护本结构体中下面定义的变量
  int32 valid;  // inode 是否被从磁盘读取了

  int16 type;
  int16 major;
  int16 minor;
  int16 nlink;
  uint32 size;
  uint32 addrs[NDIRECT+1];
};

// 将主设备号映射到设备函数的表
struct devsw {
  int32 (*read)(struct inode*, char*, int32);
  int32 (*write)(struct inode*, char*, int32);
};

extern struct devsw[];

#define CONSOLE 1

