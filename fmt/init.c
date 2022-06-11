8700 // init: The initial user-level program
8701 
8702 #include "types.h"
8703 #include "stat.h"
8704 #include "user.h"
8705 #include "fcntl.h"
8706 
8707 char *argv[] = { "sh", 0 };
8708 
8709 int
8710 main(void)
8711 {
8712   int pid, wpid;
8713 
8714   if(open("console", O_RDWR) < 0){
8715     mknod("console", 1, 1);
8716     open("console", O_RDWR);
8717   }
8718   dup(0);  // stdout
8719   dup(0);  // stderr
8720 
8721   for(;;){
8722     printf(1, "init: starting sh\n");
8723     printf(1, "project0\n");
8724     pid = fork();
8725     if(pid < 0){
8726       printf(1, "init: fork failed\n");
8727       exit();
8728     }
8729     if(pid == 0){
8730       exec("sh", argv);
8731       printf(1, "init: exec sh failed\n");
8732       exit();
8733     }
8734     while((wpid=wait()) >= 0 && wpid != pid)
8735       printf(1, "zombie!\n");
8736   }
8737 }
8738 
8739 
8740 
8741 
8742 
8743 
8744 
8745 
8746 
8747 
8748 
8749 
