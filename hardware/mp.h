struct mp {
  uchar signature[4];  // "_MP_"
  void *physaddr;  // MP 配置表的物理地址
  uchar length;
  uchar specrev;
  uchar checksum;
  uchar type;     // MP 系统配置类型
  uchar imcrp;
  uchar reserved[3];
};

struct mpconf {  // 配置表表头
  uchar signature[4];  // "PCMP"
  uint16 length;       // 表的总长度
  uchar version;       //
  uchar checksum;      //
  uchar product[20];   //
  uint32* oemtable;    // OEM 表指针
  uint16 oemlength;    // OEM 表长度
  uint16 entry;        // 表条目数量
  uint32 *lapicaddr;   // local APIC 地址
  uint16 xlength;      // 拓展表长度
  uchar xchecksum;     // 拓展表 checksum
  uchar reserved;
};

struct mpproc {  // 处理器表条目
  uchar type;          // 条目类型
  uchar apicid;        // local APIC id
  uchar version;       // local APIC version
  uchar flags;         // CPU 标志位
#define MPBOOT 0x02  // 标志这个处理器是启动处理器
  uchar signature[4];
  uint32 feature;
  uchar reserved[8];
};

struct mpioapic {  // I/O APIC 表条目
  uchar type;     // 条目类型
  uchar apicno;   // I/O APIC id
  uchar version;  // I/O APIC 版本
  uchar flags;    // I/O APIC 标志位
  uint32 *addr;   // I/O APIC 地址
};

// 表条目类型
#define MPPROC   0x00
#define MPBUS    0x01
#define MPIOAPIC 0x02
#define MPIOINTR 0x03
#define MPLINTR  0x04

