6150 //
6151 // File-system system calls.
6152 // Mostly argument checking, since we don't trust
6153 // user code, and calls into file.c and fs.c.
6154 //
6155 
6156 #include "types.h"
6157 #include "defs.h"
6158 #include "param.h"
6159 #include "stat.h"
6160 #include "mmu.h"
6161 #include "proc.h"
6162 #include "fs.h"
6163 #include "spinlock.h"
6164 #include "sleeplock.h"
6165 #include "file.h"
6166 #include "fcntl.h"
6167 
6168 // Fetch the nth word-sized system call argument as a file descriptor
6169 // and return both the descriptor and the corresponding struct file.
6170 static int
6171 argfd(int n, int *pfd, struct file **pf)
6172 {
6173   int fd;
6174   struct file *f;
6175 
6176   if(argint(n, &fd) < 0)
6177     return -1;
6178   if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
6179     return -1;
6180   if(pfd)
6181     *pfd = fd;
6182   if(pf)
6183     *pf = f;
6184   return 0;
6185 }
6186 
6187 
6188 
6189 
6190 
6191 
6192 
6193 
6194 
6195 
6196 
6197 
6198 
6199 
6200 // Allocate a file descriptor for the given file.
6201 // Takes over file reference from caller on success.
6202 static int
6203 fdalloc(struct file *f)
6204 {
6205   int fd;
6206   struct proc *curproc = myproc();
6207 
6208   for(fd = 0; fd < NOFILE; fd++){
6209     if(curproc->ofile[fd] == 0){
6210       curproc->ofile[fd] = f;
6211       return fd;
6212     }
6213   }
6214   return -1;
6215 }
6216 
6217 int
6218 sys_dup(void)
6219 {
6220   struct file *f;
6221   int fd;
6222 
6223   if(argfd(0, 0, &f) < 0)
6224     return -1;
6225   if((fd=fdalloc(f)) < 0)
6226     return -1;
6227   filedup(f);
6228   return fd;
6229 }
6230 
6231 int
6232 sys_read(void)
6233 {
6234   struct file *f;
6235   int n;
6236   char *p;
6237 
6238   if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
6239     return -1;
6240   return fileread(f, p, n);
6241 }
6242 
6243 
6244 
6245 
6246 
6247 
6248 
6249 
6250 int
6251 sys_write(void)
6252 {
6253   struct file *f;
6254   int n;
6255   char *p;
6256 
6257   if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
6258     return -1;
6259   return filewrite(f, p, n);
6260 }
6261 
6262 int
6263 sys_close(void)
6264 {
6265   int fd;
6266   struct file *f;
6267 
6268   if(argfd(0, &fd, &f) < 0)
6269     return -1;
6270   myproc()->ofile[fd] = 0;
6271   fileclose(f);
6272   return 0;
6273 }
6274 
6275 int
6276 sys_fstat(void)
6277 {
6278   struct file *f;
6279   struct stat *st;
6280 
6281   if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
6282     return -1;
6283   return filestat(f, st);
6284 }
6285 
6286 
6287 
6288 
6289 
6290 
6291 
6292 
6293 
6294 
6295 
6296 
6297 
6298 
6299 
6300 // Create the path new as a link to the same inode as old.
6301 int
6302 sys_link(void)
6303 {
6304   char name[DIRSIZ], *new, *old;
6305   struct inode *dp, *ip;
6306 
6307   if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
6308     return -1;
6309 
6310   begin_op();
6311   if((ip = namei(old)) == 0){
6312     end_op();
6313     return -1;
6314   }
6315 
6316   ilock(ip);
6317   if(ip->type == T_DIR){
6318     iunlockput(ip);
6319     end_op();
6320     return -1;
6321   }
6322 
6323   ip->nlink++;
6324   iupdate(ip);
6325   iunlock(ip);
6326 
6327   if((dp = nameiparent(new, name)) == 0)
6328     goto bad;
6329   ilock(dp);
6330   if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
6331     iunlockput(dp);
6332     goto bad;
6333   }
6334   iunlockput(dp);
6335   iput(ip);
6336 
6337   end_op();
6338 
6339   return 0;
6340 
6341 bad:
6342   ilock(ip);
6343   ip->nlink--;
6344   iupdate(ip);
6345   iunlockput(ip);
6346   end_op();
6347   return -1;
6348 }
6349 
6350 // Is the directory dp empty except for "." and ".." ?
6351 static int
6352 isdirempty(struct inode *dp)
6353 {
6354   int off;
6355   struct dirent de;
6356 
6357   for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
6358     if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
6359       panic("isdirempty: readi");
6360     if(de.inum != 0)
6361       return 0;
6362   }
6363   return 1;
6364 }
6365 
6366 
6367 
6368 
6369 
6370 
6371 
6372 
6373 
6374 
6375 
6376 
6377 
6378 
6379 
6380 
6381 
6382 
6383 
6384 
6385 
6386 
6387 
6388 
6389 
6390 
6391 
6392 
6393 
6394 
6395 
6396 
6397 
6398 
6399 
6400 int
6401 sys_unlink(void)
6402 {
6403   struct inode *ip, *dp;
6404   struct dirent de;
6405   char name[DIRSIZ], *path;
6406   uint off;
6407 
6408   if(argstr(0, &path) < 0)
6409     return -1;
6410 
6411   begin_op();
6412   if((dp = nameiparent(path, name)) == 0){
6413     end_op();
6414     return -1;
6415   }
6416 
6417   ilock(dp);
6418 
6419   // Cannot unlink "." or "..".
6420   if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
6421     goto bad;
6422 
6423   if((ip = dirlookup(dp, name, &off)) == 0)
6424     goto bad;
6425   ilock(ip);
6426 
6427   if(ip->nlink < 1)
6428     panic("unlink: nlink < 1");
6429   if(ip->type == T_DIR && !isdirempty(ip)){
6430     iunlockput(ip);
6431     goto bad;
6432   }
6433 
6434   memset(&de, 0, sizeof(de));
6435   if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
6436     panic("unlink: writei");
6437   if(ip->type == T_DIR){
6438     dp->nlink--;
6439     iupdate(dp);
6440   }
6441   iunlockput(dp);
6442 
6443   ip->nlink--;
6444   iupdate(ip);
6445   iunlockput(ip);
6446 
6447   end_op();
6448 
6449   return 0;
6450 bad:
6451   iunlockput(dp);
6452   end_op();
6453   return -1;
6454 }
6455 
6456 static struct inode*
6457 create(char *path, short type, short major, short minor)
6458 {
6459   uint off;
6460   struct inode *ip, *dp;
6461   char name[DIRSIZ];
6462 
6463   if((dp = nameiparent(path, name)) == 0)
6464     return 0;
6465   ilock(dp);
6466 
6467   if((ip = dirlookup(dp, name, &off)) != 0){
6468     iunlockput(dp);
6469     ilock(ip);
6470     if(type == T_FILE && ip->type == T_FILE)
6471       return ip;
6472     iunlockput(ip);
6473     return 0;
6474   }
6475 
6476   if((ip = ialloc(dp->dev, type)) == 0)
6477     panic("create: ialloc");
6478 
6479   ilock(ip);
6480   ip->major = major;
6481   ip->minor = minor;
6482   ip->nlink = 1;
6483   iupdate(ip);
6484 
6485   if(type == T_DIR){  // Create . and .. entries.
6486     dp->nlink++;  // for ".."
6487     iupdate(dp);
6488     // No ip->nlink++ for ".": avoid cyclic ref count.
6489     if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
6490       panic("create dots");
6491   }
6492 
6493   if(dirlink(dp, name, ip->inum) < 0)
6494     panic("create: dirlink");
6495 
6496   iunlockput(dp);
6497 
6498   return ip;
6499 }
6500 int
6501 sys_open(void)
6502 {
6503   char *path;
6504   int fd, omode;
6505   struct file *f;
6506   struct inode *ip;
6507 
6508   if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
6509     return -1;
6510 
6511   begin_op();
6512 
6513   if(omode & O_CREATE){
6514     ip = create(path, T_FILE, 0, 0);
6515     if(ip == 0){
6516       end_op();
6517       return -1;
6518     }
6519   } else {
6520     if((ip = namei(path)) == 0){
6521       end_op();
6522       return -1;
6523     }
6524     ilock(ip);
6525     if(ip->type == T_DIR && omode != O_RDONLY){
6526       iunlockput(ip);
6527       end_op();
6528       return -1;
6529     }
6530   }
6531 
6532   if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
6533     if(f)
6534       fileclose(f);
6535     iunlockput(ip);
6536     end_op();
6537     return -1;
6538   }
6539   iunlock(ip);
6540   end_op();
6541 
6542   f->type = FD_INODE;
6543   f->ip = ip;
6544   f->off = 0;
6545   f->readable = !(omode & O_WRONLY);
6546   f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
6547   return fd;
6548 }
6549 
6550 int
6551 sys_mkdir(void)
6552 {
6553   char *path;
6554   struct inode *ip;
6555 
6556   begin_op();
6557   if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
6558     end_op();
6559     return -1;
6560   }
6561   iunlockput(ip);
6562   end_op();
6563   return 0;
6564 }
6565 
6566 int
6567 sys_mknod(void)
6568 {
6569   struct inode *ip;
6570   char *path;
6571   int major, minor;
6572 
6573   begin_op();
6574   if((argstr(0, &path)) < 0 ||
6575      argint(1, &major) < 0 ||
6576      argint(2, &minor) < 0 ||
6577      (ip = create(path, T_DEV, major, minor)) == 0){
6578     end_op();
6579     return -1;
6580   }
6581   iunlockput(ip);
6582   end_op();
6583   return 0;
6584 }
6585 
6586 
6587 
6588 
6589 
6590 
6591 
6592 
6593 
6594 
6595 
6596 
6597 
6598 
6599 
6600 int
6601 sys_chdir(void)
6602 {
6603   char *path;
6604   struct inode *ip;
6605   struct proc *curproc = myproc();
6606 
6607   begin_op();
6608   if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
6609     end_op();
6610     return -1;
6611   }
6612   ilock(ip);
6613   if(ip->type != T_DIR){
6614     iunlockput(ip);
6615     end_op();
6616     return -1;
6617   }
6618   iunlock(ip);
6619   iput(curproc->cwd);
6620   end_op();
6621   curproc->cwd = ip;
6622   return 0;
6623 }
6624 
6625 int
6626 sys_exec(void)
6627 {
6628   char *path, *argv[MAXARG];
6629   int i;
6630   uint uargv, uarg;
6631 
6632   if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
6633     return -1;
6634   }
6635   memset(argv, 0, sizeof(argv));
6636   for(i=0;; i++){
6637     if(i >= NELEM(argv))
6638       return -1;
6639     if(fetchint(uargv+4*i, (int*)&uarg) < 0)
6640       return -1;
6641     if(uarg == 0){
6642       argv[i] = 0;
6643       break;
6644     }
6645     if(fetchstr(uarg, &argv[i]) < 0)
6646       return -1;
6647   }
6648   return exec(path, argv);
6649 }
6650 int
6651 sys_pipe(void)
6652 {
6653   int *fd;
6654   struct file *rf, *wf;
6655   int fd0, fd1;
6656 
6657   if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
6658     return -1;
6659   if(pipealloc(&rf, &wf) < 0)
6660     return -1;
6661   fd0 = -1;
6662   if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
6663     if(fd0 >= 0)
6664       myproc()->ofile[fd0] = 0;
6665     fileclose(rf);
6666     fileclose(wf);
6667     return -1;
6668   }
6669   fd[0] = fd0;
6670   fd[1] = fd1;
6671   return 0;
6672 }
6673 
6674 int sys_swapread(void)
6675 {
6676 	char* ptr;
6677 	int blkno;
6678 
6679 	if(argptr(0, &ptr, PGSIZE) < 0 || argint(1, &blkno) < 0 )
6680 		return -1;
6681 
6682 	swapread(ptr, blkno);
6683 	return 0;
6684 }
6685 
6686 int sys_swapwrite(void)
6687 {
6688 	char* ptr;
6689 	int blkno;
6690 
6691 	if(argptr(0, &ptr, PGSIZE) < 0 || argint(1, &blkno) < 0 )
6692 		return -1;
6693 
6694 	swapwrite(ptr, blkno);
6695 	return 0;
6696 }
6697 
6698 
6699 
