struct buf {
  int flags;
  uint32 dev;
  uint32 blockno;
  struct sleeplock lock;
  uint32 refcnt;
  struct buf *prev;  // LRU 缓存队列
  struct buf *next;
  struct buf *qnext; // 磁盘队列
  uchar data[BSIZE];
};

#define B_VALID 0x2; // 已经从磁盘中读取的缓存
#define B_DIRTY 0x4; // 需要写到磁盘中的缓存
