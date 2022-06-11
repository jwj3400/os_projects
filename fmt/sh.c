8750 // Shell.
8751 
8752 #include "types.h"
8753 #include "user.h"
8754 #include "fcntl.h"
8755 
8756 // Parsed command representation
8757 #define EXEC  1
8758 #define REDIR 2
8759 #define PIPE  3
8760 #define LIST  4
8761 #define BACK  5
8762 
8763 #define MAXARGS 10
8764 
8765 struct cmd {
8766   int type;
8767 };
8768 
8769 struct execcmd {
8770   int type;
8771   char *argv[MAXARGS];
8772   char *eargv[MAXARGS];
8773 };
8774 
8775 struct redircmd {
8776   int type;
8777   struct cmd *cmd;
8778   char *file;
8779   char *efile;
8780   int mode;
8781   int fd;
8782 };
8783 
8784 struct pipecmd {
8785   int type;
8786   struct cmd *left;
8787   struct cmd *right;
8788 };
8789 
8790 struct listcmd {
8791   int type;
8792   struct cmd *left;
8793   struct cmd *right;
8794 };
8795 
8796 struct backcmd {
8797   int type;
8798   struct cmd *cmd;
8799 };
8800 int fork1(void);  // Fork but panics on failure.
8801 void panic(char*);
8802 struct cmd *parsecmd(char*);
8803 
8804 // Execute cmd.  Never returns.
8805 void
8806 runcmd(struct cmd *cmd)
8807 {
8808   int p[2];
8809   struct backcmd *bcmd;
8810   struct execcmd *ecmd;
8811   struct listcmd *lcmd;
8812   struct pipecmd *pcmd;
8813   struct redircmd *rcmd;
8814 
8815   if(cmd == 0)
8816     exit();
8817 
8818   switch(cmd->type){
8819   default:
8820     panic("runcmd");
8821 
8822   case EXEC:
8823     ecmd = (struct execcmd*)cmd;
8824     if(ecmd->argv[0] == 0)
8825       exit();
8826     exec(ecmd->argv[0], ecmd->argv);
8827     printf(2, "exec %s failed\n", ecmd->argv[0]);
8828     break;
8829 
8830   case REDIR:
8831     rcmd = (struct redircmd*)cmd;
8832     close(rcmd->fd);
8833     if(open(rcmd->file, rcmd->mode) < 0){
8834       printf(2, "open %s failed\n", rcmd->file);
8835       exit();
8836     }
8837     runcmd(rcmd->cmd);
8838     break;
8839 
8840   case LIST:
8841     lcmd = (struct listcmd*)cmd;
8842     if(fork1() == 0)
8843       runcmd(lcmd->left);
8844     wait();
8845     runcmd(lcmd->right);
8846     break;
8847 
8848 
8849 
8850   case PIPE:
8851     pcmd = (struct pipecmd*)cmd;
8852     if(pipe(p) < 0)
8853       panic("pipe");
8854     if(fork1() == 0){
8855       close(1);
8856       dup(p[1]);
8857       close(p[0]);
8858       close(p[1]);
8859       runcmd(pcmd->left);
8860     }
8861     if(fork1() == 0){
8862       close(0);
8863       dup(p[0]);
8864       close(p[0]);
8865       close(p[1]);
8866       runcmd(pcmd->right);
8867     }
8868     close(p[0]);
8869     close(p[1]);
8870     wait();
8871     wait();
8872     break;
8873 
8874   case BACK:
8875     bcmd = (struct backcmd*)cmd;
8876     if(fork1() == 0)
8877       runcmd(bcmd->cmd);
8878     break;
8879   }
8880   exit();
8881 }
8882 
8883 int
8884 getcmd(char *buf, int nbuf)
8885 {
8886   printf(2, "$ ");
8887   memset(buf, 0, nbuf);
8888   gets(buf, nbuf);
8889   if(buf[0] == 0) // EOF
8890     return -1;
8891   return 0;
8892 }
8893 
8894 
8895 
8896 
8897 
8898 
8899 
8900 int
8901 main(void)
8902 {
8903   static char buf[100];
8904   int fd;
8905 
8906   // Ensure that three file descriptors are open.
8907   while((fd = open("console", O_RDWR)) >= 0){
8908     if(fd >= 3){
8909       close(fd);
8910       break;
8911     }
8912   }
8913 
8914   // Read and run input commands.
8915   while(getcmd(buf, sizeof(buf)) >= 0){
8916     if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
8917       // Chdir must be called by the parent, not the child.
8918       buf[strlen(buf)-1] = 0;  // chop \n
8919       if(chdir(buf+3) < 0)
8920         printf(2, "cannot cd %s\n", buf+3);
8921       continue;
8922     }
8923     if(fork1() == 0)
8924       runcmd(parsecmd(buf));
8925     wait();
8926   }
8927   exit();
8928 }
8929 
8930 void
8931 panic(char *s)
8932 {
8933   printf(2, "%s\n", s);
8934   exit();
8935 }
8936 
8937 int
8938 fork1(void)
8939 {
8940   int pid;
8941 
8942   pid = fork();
8943   if(pid == -1)
8944     panic("fork");
8945   return pid;
8946 }
8947 
8948 
8949 
8950 // Constructors
8951 
8952 struct cmd*
8953 execcmd(void)
8954 {
8955   struct execcmd *cmd;
8956 
8957   cmd = malloc(sizeof(*cmd));
8958   memset(cmd, 0, sizeof(*cmd));
8959   cmd->type = EXEC;
8960   return (struct cmd*)cmd;
8961 }
8962 
8963 struct cmd*
8964 redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
8965 {
8966   struct redircmd *cmd;
8967 
8968   cmd = malloc(sizeof(*cmd));
8969   memset(cmd, 0, sizeof(*cmd));
8970   cmd->type = REDIR;
8971   cmd->cmd = subcmd;
8972   cmd->file = file;
8973   cmd->efile = efile;
8974   cmd->mode = mode;
8975   cmd->fd = fd;
8976   return (struct cmd*)cmd;
8977 }
8978 
8979 struct cmd*
8980 pipecmd(struct cmd *left, struct cmd *right)
8981 {
8982   struct pipecmd *cmd;
8983 
8984   cmd = malloc(sizeof(*cmd));
8985   memset(cmd, 0, sizeof(*cmd));
8986   cmd->type = PIPE;
8987   cmd->left = left;
8988   cmd->right = right;
8989   return (struct cmd*)cmd;
8990 }
8991 
8992 
8993 
8994 
8995 
8996 
8997 
8998 
8999 
9000 struct cmd*
9001 listcmd(struct cmd *left, struct cmd *right)
9002 {
9003   struct listcmd *cmd;
9004 
9005   cmd = malloc(sizeof(*cmd));
9006   memset(cmd, 0, sizeof(*cmd));
9007   cmd->type = LIST;
9008   cmd->left = left;
9009   cmd->right = right;
9010   return (struct cmd*)cmd;
9011 }
9012 
9013 struct cmd*
9014 backcmd(struct cmd *subcmd)
9015 {
9016   struct backcmd *cmd;
9017 
9018   cmd = malloc(sizeof(*cmd));
9019   memset(cmd, 0, sizeof(*cmd));
9020   cmd->type = BACK;
9021   cmd->cmd = subcmd;
9022   return (struct cmd*)cmd;
9023 }
9024 
9025 
9026 
9027 
9028 
9029 
9030 
9031 
9032 
9033 
9034 
9035 
9036 
9037 
9038 
9039 
9040 
9041 
9042 
9043 
9044 
9045 
9046 
9047 
9048 
9049 
9050 // Parsing
9051 
9052 char whitespace[] = " \t\r\n\v";
9053 char symbols[] = "<|>&;()";
9054 
9055 int
9056 gettoken(char **ps, char *es, char **q, char **eq)
9057 {
9058   char *s;
9059   int ret;
9060 
9061   s = *ps;
9062   while(s < es && strchr(whitespace, *s))
9063     s++;
9064   if(q)
9065     *q = s;
9066   ret = *s;
9067   switch(*s){
9068   case 0:
9069     break;
9070   case '|':
9071   case '(':
9072   case ')':
9073   case ';':
9074   case '&':
9075   case '<':
9076     s++;
9077     break;
9078   case '>':
9079     s++;
9080     if(*s == '>'){
9081       ret = '+';
9082       s++;
9083     }
9084     break;
9085   default:
9086     ret = 'a';
9087     while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
9088       s++;
9089     break;
9090   }
9091   if(eq)
9092     *eq = s;
9093 
9094   while(s < es && strchr(whitespace, *s))
9095     s++;
9096   *ps = s;
9097   return ret;
9098 }
9099 
9100 int
9101 peek(char **ps, char *es, char *toks)
9102 {
9103   char *s;
9104 
9105   s = *ps;
9106   while(s < es && strchr(whitespace, *s))
9107     s++;
9108   *ps = s;
9109   return *s && strchr(toks, *s);
9110 }
9111 
9112 struct cmd *parseline(char**, char*);
9113 struct cmd *parsepipe(char**, char*);
9114 struct cmd *parseexec(char**, char*);
9115 struct cmd *nulterminate(struct cmd*);
9116 
9117 struct cmd*
9118 parsecmd(char *s)
9119 {
9120   char *es;
9121   struct cmd *cmd;
9122 
9123   es = s + strlen(s);
9124   cmd = parseline(&s, es);
9125   peek(&s, es, "");
9126   if(s != es){
9127     printf(2, "leftovers: %s\n", s);
9128     panic("syntax");
9129   }
9130   nulterminate(cmd);
9131   return cmd;
9132 }
9133 
9134 struct cmd*
9135 parseline(char **ps, char *es)
9136 {
9137   struct cmd *cmd;
9138 
9139   cmd = parsepipe(ps, es);
9140   while(peek(ps, es, "&")){
9141     gettoken(ps, es, 0, 0);
9142     cmd = backcmd(cmd);
9143   }
9144   if(peek(ps, es, ";")){
9145     gettoken(ps, es, 0, 0);
9146     cmd = listcmd(cmd, parseline(ps, es));
9147   }
9148   return cmd;
9149 }
9150 struct cmd*
9151 parsepipe(char **ps, char *es)
9152 {
9153   struct cmd *cmd;
9154 
9155   cmd = parseexec(ps, es);
9156   if(peek(ps, es, "|")){
9157     gettoken(ps, es, 0, 0);
9158     cmd = pipecmd(cmd, parsepipe(ps, es));
9159   }
9160   return cmd;
9161 }
9162 
9163 struct cmd*
9164 parseredirs(struct cmd *cmd, char **ps, char *es)
9165 {
9166   int tok;
9167   char *q, *eq;
9168 
9169   while(peek(ps, es, "<>")){
9170     tok = gettoken(ps, es, 0, 0);
9171     if(gettoken(ps, es, &q, &eq) != 'a')
9172       panic("missing file for redirection");
9173     switch(tok){
9174     case '<':
9175       cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
9176       break;
9177     case '>':
9178       cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
9179       break;
9180     case '+':  // >>
9181       cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
9182       break;
9183     }
9184   }
9185   return cmd;
9186 }
9187 
9188 
9189 
9190 
9191 
9192 
9193 
9194 
9195 
9196 
9197 
9198 
9199 
9200 struct cmd*
9201 parseblock(char **ps, char *es)
9202 {
9203   struct cmd *cmd;
9204 
9205   if(!peek(ps, es, "("))
9206     panic("parseblock");
9207   gettoken(ps, es, 0, 0);
9208   cmd = parseline(ps, es);
9209   if(!peek(ps, es, ")"))
9210     panic("syntax - missing )");
9211   gettoken(ps, es, 0, 0);
9212   cmd = parseredirs(cmd, ps, es);
9213   return cmd;
9214 }
9215 
9216 struct cmd*
9217 parseexec(char **ps, char *es)
9218 {
9219   char *q, *eq;
9220   int tok, argc;
9221   struct execcmd *cmd;
9222   struct cmd *ret;
9223 
9224   if(peek(ps, es, "("))
9225     return parseblock(ps, es);
9226 
9227   ret = execcmd();
9228   cmd = (struct execcmd*)ret;
9229 
9230   argc = 0;
9231   ret = parseredirs(ret, ps, es);
9232   while(!peek(ps, es, "|)&;")){
9233     if((tok=gettoken(ps, es, &q, &eq)) == 0)
9234       break;
9235     if(tok != 'a')
9236       panic("syntax");
9237     cmd->argv[argc] = q;
9238     cmd->eargv[argc] = eq;
9239     argc++;
9240     if(argc >= MAXARGS)
9241       panic("too many args");
9242     ret = parseredirs(ret, ps, es);
9243   }
9244   cmd->argv[argc] = 0;
9245   cmd->eargv[argc] = 0;
9246   return ret;
9247 }
9248 
9249 
9250 // NUL-terminate all the counted strings.
9251 struct cmd*
9252 nulterminate(struct cmd *cmd)
9253 {
9254   int i;
9255   struct backcmd *bcmd;
9256   struct execcmd *ecmd;
9257   struct listcmd *lcmd;
9258   struct pipecmd *pcmd;
9259   struct redircmd *rcmd;
9260 
9261   if(cmd == 0)
9262     return 0;
9263 
9264   switch(cmd->type){
9265   case EXEC:
9266     ecmd = (struct execcmd*)cmd;
9267     for(i=0; ecmd->argv[i]; i++)
9268       *ecmd->eargv[i] = 0;
9269     break;
9270 
9271   case REDIR:
9272     rcmd = (struct redircmd*)cmd;
9273     nulterminate(rcmd->cmd);
9274     *rcmd->efile = 0;
9275     break;
9276 
9277   case PIPE:
9278     pcmd = (struct pipecmd*)cmd;
9279     nulterminate(pcmd->left);
9280     nulterminate(pcmd->right);
9281     break;
9282 
9283   case LIST:
9284     lcmd = (struct listcmd*)cmd;
9285     nulterminate(lcmd->left);
9286     nulterminate(lcmd->right);
9287     break;
9288 
9289   case BACK:
9290     bcmd = (struct backcmd*)cmd;
9291     nulterminate(bcmd->cmd);
9292     break;
9293   }
9294   return cmd;
9295 }
9296 
9297 
9298 
9299 
