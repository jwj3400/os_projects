3100 # Context switch
3101 #
3102 #   void swtch(struct context **old, struct context *new);
3103 #
3104 # Save the current registers on the stack, creating
3105 # a struct context, and save its address in *old.
3106 # Switch stacks to new and pop previously-saved registers.
3107 
3108 .globl swtch
3109 swtch:
3110   movl 4(%esp), %eax
3111   movl 8(%esp), %edx
3112 
3113   # Save old callee-saved registers
3114   pushl %ebp
3115   pushl %ebx
3116   pushl %esi
3117   pushl %edi
3118 
3119   # Switch stacks
3120   movl %esp, (%eax)
3121   movl %edx, %esp
3122 
3123   # Load new callee-saved registers
3124   popl %edi
3125   popl %esi
3126   popl %ebx
3127   popl %ebp
3128   ret
3129 
3130 
3131 
3132 
3133 
3134 
3135 
3136 
3137 
3138 
3139 
3140 
3141 
3142 
3143 
3144 
3145 
3146 
3147 
3148 
3149 
