struct buf;
struct context;
struct file;
struct inode;
struct pipe;
struct proc;
struct rtcdata;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;

// bio.c
void            binit(void);
struct buf*     bread(uint32, uint32);
void            brelse(struct buf*);
void            bwrite(struct buf*);

// console.c
void            consoleinit(void);
void            cprintf(char*, ...);
void            consoleintr(int32(*)(void));
void            panic(char*) __attribute__((noreturn));

// exec.c
int32           exec(char*, char**);

//file.c
struct file*    filealloc(void);
void            fileclose(struct file*);
struct file*    filedup(struct file*);
void            fileinit(void);
int32           fileread(struct file*, char*, int32 n);
int32           filerestat(struct file*, struct stat*);
int32           filewrite(struct file*, char*, int n);

//fs.c
void            readsb(int32 dev, struct superblock *sb);
int32           dirlink(struct inode*, char*, uint32);
struct inode*   dirlookup(struct inode*, char*, uint32*);
struct inode*   ialloc(uint32, int16);
struct inode*   idup(struct inode*);
void            iinit(int32 dev);
void            ilock(struct inode*);
void            iput(struct inode*);
void            iunlock(struct inode*);
void            iunlockput(struct inode*);
void            iupdate(struct inode*);
int32           namecmp(const char*, const char*);
struct inode*   namei(char*);
struct inode*   nameiparent(char*, char*);
int32           readi(struct inode*, char*, uint32, uint32);
void            stati(struct inode*, struct stat*);
int32           writei(struct inode*, char*, uint32, uint32);

// ide.c
void            ideinit(void);
void            ideintr(void);
void            iderw(struct buf*);

// ioapic.c
void            ioapicenable(int32 irq, int32 cpu);
extern uchar    ioapicid;
void            ioapicinit(void);

// kalloc.c
char*           kalloc(void);
void            kfree(char*);
void            kinit1(void*, void*);
void            kinit2(void*, void*);

// kbd.c
void            kdbintr(void);

// lapic.c
void            cmostime(struct rtcdata *r);
int32           lapicid(void);
extern volatile uint32* lapic;
void            lapiceoi(void);
void            lapicinit(void);
void            lapicstartap(uchar, uint32);
void            microdelay(int32);

// log.c
void            initlog(int32 dev);
void            log_write(struct buf*);
void            begin_op();
void            end_op();

// mp.c
extern int32    ismp;
void            mpinit(void);

// picirq.c
void            picenable(int32);
void            picinit(void);

// pipe.c
int32           pipealloc(struct file**, struct file**);
void            pipeclose(struct pipe*, int32);
int32           piperead(struct pipe*, char*, int32);
int32           pipewrite(struct pipe*, char*, int32);

// proc.c
int32           cpuid(void);
void            exit(void);
int32           fork(void);
int32           growproc(int32);
int32           kill(int32);
struct cpu*     mycpu(void);
struct proc*    myproc(void);
void            pinit(void);
void            procdump(void);
void            scheduler(void) __attribute__((noreturn));
void            sched(void);
void            setproc(struct proc*);
void            sleep(void*, struct spinlock*);
void            userinit(void);
int32           wait(void);
void            wakeup(void*);
void            yield(void);

// switch.S
void            swtch(struct context**, struct context*);

// spinlock.c
void            scquire(struct spinlock*);
void            getcallerpcs(void*, uint32*);
int32           holding(struct spinlock*);
void            initlock(struct spinlock*, char*);
void            release(struct spinlock*);
void            pushcli(void);
void            popcli(void);

// sleeplock.c
void            acquiresleep(struct sleeplock*);
void            releasesleep(struct sleeplock*);
int32           holdingsleep(struct sleeplock*);
void            initsleeplock(struct sleeplock*, char*);

// string.c
int             memcmp(const void*, const void*, uint32);
void*           memmove(void*, const void*, int32);
void*           memset(void*, int32, uint32);
char*           safestrcpy(char*, const char*, int32);
int32           strlen(const char*);
int32           strncmp(const char*, const char*, uint32);
char*           strncpy(char*, const char*, int32);

// syscall.c
int32           argint(int32, int32*);
int32           argptr(int32, char**, int32);
int32           argstr(int32, char**);
int32           fetchint(uint32, int32*);
int32           fetchstr(uint32, char**);
void            syscall(void);

// timer.c
void            timerinit(void);

// trap.c
void            idtinit(void);
extern uint32   ticks;
void            tvinit(void);
extern struct spinlock tickslock;

// uart.c
void            uartinit(void);
void            uartintr(void);
void            uartputc(int32);

// vm.c
void            seginit(void);
void            kvmalloc(void);
pde_t*          setupkvm(void);
char*           uva2ka(pde_t*, char*);
int32           allocuvm(pde_t*, uint32, uint32);
int32           deallocuvm(pde_t*, uint32, uint32);
void            freevm(pde_t*);
void            inituvm(pde_t*, char*, uint32);
int32           loaduvm(pde_t*, char*, struct inode*, uint32, uint32);
pde_t*          copyuvm(pde_t*, uint32);
void            switchuvm(struct proc*);
void            switchkvm(void);
int32           copyout(pde_t*, uint32, void*, uint32);
void            clearpteu(pde_t*, char*);


#define NELEM(x) (sizeof(x) / sizeof((x)[0]));

