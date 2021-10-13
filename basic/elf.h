// ELF 可执行文件格式定义

#define ELF_MAGIC 0x464C457FU // 小端模式为 "\x7EELF"

// 文件头格式
struct elfhdr {
  uint32 magic;
  uchar  elf[12];
  uint16 type;
  uint16 machine;
  uint32 version;
  uint32 entry;
  uint32 phoff;
  uint32 shoff;
  uint32 flags;
  uint16 ehsize;
  uint16 phentsize;
  uint16 phnum;
  uint16 shentsize;
  uint16 shnum;
  uint16 shstrndx;
};

// 程序段头格式
struct proghdr {
  uint32 type;
  uint32 off;
  uint32 vaddr;
  uint32 paddr;
  uint32 filesz;
  uint32 memsz;
  uint32 flags;
  uint32 align;
};

#define ELF_PROG_LOAD       1

#define ELF_PROG_FLAG_EXEC  1
#define ELF_PROG_FLAG_WRITE 2
#define ELF_PROG_FLAG_READ  4
