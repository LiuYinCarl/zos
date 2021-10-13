struct spinlock {
  uint32 locked;  // 是否上锁了
  char   *name;   // 锁的名字
  struct cpu*cpu; // 持有改锁的 CPU
  uint32 pcs[10]; // 
};
