#define T_DIR  1  // 目录
#define T_FILE 2  // 文件
#define T_DEV  3  // 设备

struct stat {
  int16  type;  // 文件类型
  int32  dev;   // 文件系统的磁盘设备
  uint32 ino;   // Inode number
  int16  nlink; // 指向文件的链接数
  uint32 size;  // 文件大小，以字节为单位
};

