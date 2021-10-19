// 磁盘文件系统

#define ROOTINO 1 // root i-number
#define BSIZE 512 // block size

// 磁盘布局
// [boot block | super block | log | inode blocks | free bit map | data blocks ]

// 超级块描述了磁盘的布局
struct superblocks {
  uint32 size;       // 文件系统镜像大小
  uint32 nblocks;    // 数据块数量
  uint32 ninodes;    // inode 数量
  uint32 nlog;       // 日志块数量
  uint32 logstart;   // 第一个日志块的位置
  uint32 inodestart; // 第一个 inode 块的位置
  uint32 bmapstart;  //
};

#define NDIRECT 12
#define NINDIRECT (BASIC / sizeof(uint32))
#define MAXFILE (NDIRECT + NINDIRECT)

// 磁盘上的 inode 结构
struct dinode {
  int16 type;  // 文件类型
  int16 major; // 主设备号
  int16 minor; // 次设备号
  int16 nlink; // 文件系统中指向 inode 的链接数
  uint32 size; // 文件的字节数大小
  uint32 addrs[NDIRECT+1]; // 数据块地址
};

// 每个 block 的 Inodes
#define IPB (BSIZE / sizeof(struct dinode))

// 包含 inode i 的 block
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)

// 每个 block 的 位图
#define BPB (BSIZE*8)

#define BBLOCK(b, sb) (b/BPB + sb.bmapstart)

#define DIRSIZ 14

struct dirent {
  uint16 inum;
  char name[DIRSIZ];
};

