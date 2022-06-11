2400 #include "types.h"
2401 #include "defs.h"
2402 #include "param.h"
2403 #include "memlayout.h"
2404 #include "mmu.h"
2405 #include "x86.h"
2406 #include "proc.h"
2407 #include "spinlock.h"
2408 
2409 typedef struct pt {
2410   struct spinlock lock;
2411   struct proc proc[NPROC];
2412 } Ptable;
2413 
2414 Ptable ptable;
2415 
2416 
2417 static struct proc *initproc;
2418 
2419 int nextpid = 1;
2420 extern void forkret(void);
2421 extern void trapret(void);
2422 
2423 static void wakeup1(void *chan);
2424 
2425 void
2426 pinit(void)
2427 {
2428   initlock(&ptable.lock, "ptable");
2429 }
2430 
2431 // Must be called with interrupts disabled
2432 int
2433 cpuid() {
2434   return mycpu()-cpus;
2435 }
2436 
2437 
2438 
2439 
2440 
2441 
2442 
2443 
2444 
2445 
2446 
2447 
2448 
2449 
2450 // Must be called with interrupts disabled to avoid the caller being
2451 // rescheduled between reading lapicid and running through the loop.
2452 struct cpu*
2453 mycpu(void)
2454 {
2455   int apicid, i;
2456 
2457   if(readeflags()&FL_IF)
2458     panic("mycpu called with interrupts enabled\n");
2459 
2460   apicid = lapicid();
2461   // APIC IDs are not guaranteed to be contiguous. Maybe we should have
2462   // a reverse map, or reserve a register to store &cpus[i].
2463   for (i = 0; i < ncpu; ++i) {
2464     if (cpus[i].apicid == apicid)
2465       return &cpus[i];
2466   }
2467   panic("unknown apicid\n");
2468 }
2469 
2470 // Disable interrupts so that we are not rescheduled
2471 // while reading proc from the cpu structure
2472 struct proc*
2473 myproc(void) {
2474   struct cpu *c;
2475   struct proc *p;
2476   pushcli();
2477   c = mycpu();
2478   p = c->proc;
2479   popcli();
2480   return p;
2481 }
2482 
2483 
2484 
2485 
2486 
2487 
2488 
2489 
2490 
2491 
2492 
2493 
2494 
2495 
2496 
2497 
2498 
2499 
2500 // Look in the process table for an UNUSED proc.
2501 // If found, change state to EMBRYO and initialize
2502 // state required to run in the kernel.
2503 // Otherwise return 0.
2504 static struct proc*
2505 allocproc(void)
2506 {
2507   struct proc *p;
2508   char *sp;
2509 
2510   acquire(&ptable.lock);
2511 
2512   for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
2513     if(p->state == UNUSED)
2514       goto found;
2515 
2516   release(&ptable.lock);
2517   return 0;
2518 
2519 found:
2520   p->state = EMBRYO;
2521   p->pid = nextpid++;
2522 
2523   release(&ptable.lock);
2524 
2525   // Allocate kernel stack.
2526   if((p->kstack = kalloc()) == 0){
2527     p->state = UNUSED;
2528     return 0;
2529   }
2530   sp = p->kstack + KSTACKSIZE;
2531 
2532   // Leave room for trap frame.
2533   sp -= sizeof *p->tf;
2534   p->tf = (struct trapframe*)sp;
2535 
2536   // Set up new context to start executing at forkret,
2537   // which returns to trapret.
2538   sp -= 4;
2539   *(uint*)sp = (uint)trapret;
2540 
2541   sp -= sizeof *p->context;
2542   p->context = (struct context*)sp;
2543   memset(p->context, 0, sizeof *p->context);
2544   p->context->eip = (uint)forkret;
2545 
2546   return p;
2547 }
2548 
2549 
2550 // Set up first user process.
2551 void
2552 userinit(void)
2553 {
2554   struct proc *p;
2555   extern char _binary_initcode_start[], _binary_initcode_size[];
2556 
2557   p = allocproc();
2558 
2559   initproc = p;
2560   if((p->pgdir = setupkvm()) == 0)
2561     panic("userinit: out of memory?");
2562   cprintf("%p %p\n", _binary_initcode_start, _binary_initcode_size);
2563   inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
2564   p->sz = PGSIZE;
2565   memset(p->tf, 0, sizeof(*p->tf));
2566   p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
2567   p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
2568   p->tf->es = p->tf->ds;
2569   p->tf->ss = p->tf->ds;
2570   p->tf->eflags = FL_IF;
2571   p->tf->esp = PGSIZE;
2572   p->tf->eip = 0;  // beginning of initcode.S
2573 
2574   safestrcpy(p->name, "initcode", sizeof(p->name));
2575   p->cwd = namei("/");
2576 
2577   // this assignment to p->state lets other cores
2578   // run this process. the acquire forces the above
2579   // writes to be visible, and the lock is also needed
2580   // because the assignment might not be atomic.
2581   acquire(&ptable.lock);
2582 
2583   p->state = RUNNABLE;
2584   p->priority = 20;
2585   release(&ptable.lock);
2586 }
2587 
2588 
2589 
2590 
2591 
2592 
2593 
2594 
2595 
2596 
2597 
2598 
2599 
2600 // Grow current process's memory by n bytes.
2601 // Return 0 on success, -1 on failure.
2602 int
2603 growproc(int n)
2604 {
2605   uint sz;
2606   struct proc *curproc = myproc();
2607 
2608   sz = curproc->sz;
2609   if(n > 0){
2610     if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
2611       return -1;
2612   } else if(n < 0){
2613     if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
2614       return -1;
2615   }
2616   curproc->sz = sz;
2617   switchuvm(curproc);
2618   return 0;
2619 }
2620 
2621 // Create a new process copying p as the parent.
2622 // Sets up stack to return as if from system call.
2623 // Caller must set state of returned proc to RUNNABLE.
2624 int
2625 fork(void)
2626 {
2627   int i, pid;
2628   struct proc *np;
2629   struct proc *curproc = myproc();
2630 
2631   // Allocate process.
2632   if((np = allocproc()) == 0){
2633     return -1;
2634   }
2635 
2636   // Copy process state from proc.
2637   if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
2638     kfree(np->kstack);
2639     np->kstack = 0;
2640     np->state = UNUSED;
2641     return -1;
2642   }
2643   np->sz = curproc->sz;
2644   np->parent = curproc;
2645   *np->tf = *curproc->tf;
2646   np->priority = curproc->priority;
2647 
2648 
2649 
2650   // Clear %eax so that fork returns 0 in the child.
2651   np->tf->eax = 0;
2652 
2653   for(i = 0; i < NOFILE; i++)
2654     if(curproc->ofile[i])
2655       np->ofile[i] = filedup(curproc->ofile[i]);
2656   np->cwd = idup(curproc->cwd);
2657 
2658   safestrcpy(np->name, curproc->name, sizeof(curproc->name));
2659 
2660   pid = np->pid;
2661 
2662   acquire(&ptable.lock);
2663 
2664   np->state = RUNNABLE;
2665   release(&ptable.lock);
2666 
2667   return pid;
2668 }
2669 
2670 // Exit the current process.  Does not return.
2671 // An exited process remains in the zombie state
2672 // until its parent calls wait() to find out it exited.
2673 void
2674 exit(void)
2675 {
2676   struct proc *curproc = myproc();
2677   struct proc *p;
2678   int fd;
2679 
2680   if(curproc == initproc)
2681     panic("init exiting");
2682 
2683   // Close all open files.
2684   for(fd = 0; fd < NOFILE; fd++){
2685     if(curproc->ofile[fd]){
2686       fileclose(curproc->ofile[fd]);
2687       curproc->ofile[fd] = 0;
2688     }
2689   }
2690 
2691   begin_op();
2692   iput(curproc->cwd);
2693   end_op();
2694   curproc->cwd = 0;
2695 
2696   acquire(&ptable.lock);
2697 
2698   // Parent might be sleeping in wait().
2699   wakeup1(curproc->parent);
2700   // Pass abandoned children to init.
2701   for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
2702     if(p->parent == curproc){
2703       p->parent = initproc;
2704       if(p->state == ZOMBIE)
2705         wakeup1(initproc);
2706     }
2707   }
2708 
2709   // Jump into the scheduler, never to return.
2710   curproc->state = ZOMBIE;
2711   sched();
2712   panic("zombie exit");
2713 }
2714 
2715 // Wait for a child process to exit and return its pid.
2716 // Return -1 if this process has no children.
2717 int
2718 wait(void)
2719 {
2720   struct proc *p;
2721   int havekids, pid;
2722   struct proc *curproc = myproc();
2723 
2724   acquire(&ptable.lock);
2725   for(;;){
2726     // Scan through table looking for exited children.
2727     havekids = 0;
2728     for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
2729       if(p->parent != curproc)
2730         continue;
2731       havekids = 1;
2732       if(p->state == ZOMBIE){
2733         // Found one.
2734         pid = p->pid;
2735         kfree(p->kstack);
2736         p->kstack = 0;
2737         freevm(p->pgdir);
2738         p->pid = 0;
2739         p->parent = 0;
2740         p->name[0] = 0;
2741         p->killed = 0;
2742         p->state = UNUSED;
2743         release(&ptable.lock);
2744         return pid;
2745       }
2746     }
2747 
2748 
2749 
2750     // No point waiting if we don't have any children.
2751     if(!havekids || curproc->killed){
2752       release(&ptable.lock);
2753       return -1;
2754     }
2755 
2756     // Wait for children to exit.  (See wakeup1 call in proc_exit.)
2757     sleep(curproc, &ptable.lock);  //DOC: wait-sleep
2758   }
2759 }
2760 
2761 
2762 
2763 
2764 
2765 
2766 
2767 
2768 
2769 
2770 
2771 
2772 
2773 
2774 
2775 
2776 
2777 
2778 
2779 
2780 
2781 
2782 
2783 
2784 
2785 
2786 
2787 
2788 
2789 
2790 
2791 
2792 
2793 
2794 
2795 
2796 
2797 
2798 
2799 
2800 // Per-CPU process scheduler.
2801 // Each CPU calls scheduler() after setting itself up.
2802 // Scheduler never returns.  It loops, doing:
2803 //  - choose a process to run
2804 //  - swtch to start running that process
2805 //  - eventually that process transfers control
2806 //      via swtch back to the scheduler.
2807 void
2808 scheduler(void)
2809 {
2810   struct proc *p;
2811   struct cpu *c = mycpu();
2812   c->proc = 0;
2813 
2814   for(;;){
2815     // Enable interrupts on this processor.
2816     sti();
2817 
2818     // Loop over process table looking for process to run.
2819     acquire(&ptable.lock);
2820     for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
2821       if(p->state != RUNNABLE)
2822         continue;
2823 
2824       // Switch to chosen process.  It is the process's job
2825       // to release ptable.lock and then reacquire it
2826       // before jumping back to us.
2827       c->proc = p;
2828       switchuvm(p);
2829       p->state = RUNNING;
2830 
2831       swtch(&(c->scheduler), p->context);
2832       switchkvm();
2833 
2834       // Process is done running for now.
2835       // It should have changed its p->state before coming back.
2836       c->proc = 0;
2837     }
2838     release(&ptable.lock);
2839 
2840   }
2841 }
2842 
2843 
2844 
2845 
2846 
2847 
2848 
2849 
2850 // Enter scheduler.  Must hold only ptable.lock
2851 // and have changed proc->state. Saves and restores
2852 // intena because intena is a property of this
2853 // kernel thread, not this CPU. It should
2854 // be proc->intena and proc->ncli, but that would
2855 // break in the few places where a lock is held but
2856 // there's no process.
2857 void
2858 sched(void)
2859 {
2860   int intena;
2861   struct proc *p = myproc();
2862 
2863   if(!holding(&ptable.lock))
2864     panic("sched ptable.lock");
2865   if(mycpu()->ncli != 1)
2866     panic("sched locks");
2867   if(p->state == RUNNING)
2868     panic("sched running");
2869   if(readeflags()&FL_IF)
2870     panic("sched interruptible");
2871   intena = mycpu()->intena;
2872   swtch(&p->context, mycpu()->scheduler);
2873   mycpu()->intena = intena;
2874 }
2875 
2876 // Give up the CPU for one scheduling round.
2877 void
2878 yield(void)
2879 {
2880   acquire(&ptable.lock);  //DOC: yieldlock
2881   myproc()->state = RUNNABLE;
2882   sched();
2883   release(&ptable.lock);
2884 }
2885 
2886 
2887 
2888 
2889 
2890 
2891 
2892 
2893 
2894 
2895 
2896 
2897 
2898 
2899 
2900 // A fork child's very first scheduling by scheduler()
2901 // will swtch here.  "Return" to user space.
2902 void
2903 forkret(void)
2904 {
2905   static int first = 1;
2906   // Still holding ptable.lock from scheduler.
2907   release(&ptable.lock);
2908 
2909   if (first) {
2910     // Some initialization functions must be run in the context
2911     // of a regular process (e.g., they call sleep), and thus cannot
2912     // be run from main().
2913     first = 0;
2914     iinit(ROOTDEV);
2915     initlog(ROOTDEV);
2916   }
2917 
2918   // Return to "caller", actually trapret (see allocproc).
2919 }
2920 
2921 // Atomically release lock and sleep on chan.
2922 // Reacquires lock when awakened.
2923 void
2924 sleep(void *chan, struct spinlock *lk)
2925 {
2926   struct proc *p = myproc();
2927 
2928   if(p == 0)
2929     panic("sleep");
2930 
2931   if(lk == 0)
2932     panic("sleep without lk");
2933 
2934   // Must acquire ptable.lock in order to
2935   // change p->state and then call sched.
2936   // Once we hold ptable.lock, we can be
2937   // guaranteed that we won't miss any wakeup
2938   // (wakeup runs with ptable.lock locked),
2939   // so it's okay to release lk.
2940   if(lk != &ptable.lock){  //DOC: sleeplock0
2941     acquire(&ptable.lock);  //DOC: sleeplock1
2942     release(lk);
2943   }
2944   // Go to sleep.
2945   p->chan = chan;
2946   p->state = SLEEPING;
2947 
2948   sched();
2949 
2950   // Tidy up.
2951   p->chan = 0;
2952 
2953   // Reacquire original lock.
2954   if(lk != &ptable.lock){  //DOC: sleeplock2
2955     release(&ptable.lock);
2956     acquire(lk);
2957   }
2958 }
2959 
2960 
2961 
2962 
2963 
2964 
2965 
2966 
2967 
2968 
2969 
2970 
2971 
2972 
2973 
2974 
2975 
2976 
2977 
2978 
2979 
2980 
2981 
2982 
2983 
2984 
2985 
2986 
2987 
2988 
2989 
2990 
2991 
2992 
2993 
2994 
2995 
2996 
2997 
2998 
2999 
3000 // Wake up all processes sleeping on chan.
3001 // The ptable lock must be held.
3002 static void
3003 wakeup1(void *chan)
3004 {
3005   struct proc *p;
3006 
3007   for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
3008     if(p->state == SLEEPING && p->chan == chan)
3009       p->state = RUNNABLE;
3010 }
3011 
3012 // Wake up all processes sleeping on chan.
3013 void
3014 wakeup(void *chan)
3015 {
3016   acquire(&ptable.lock);
3017   wakeup1(chan);
3018   release(&ptable.lock);
3019 }
3020 
3021 // Kill the process with the given pid.
3022 // Process won't exit until it returns
3023 // to user space (see trap in trap.c).
3024 int
3025 kill(int pid)
3026 {
3027   struct proc *p;
3028 
3029   acquire(&ptable.lock);
3030   for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
3031     if(p->pid == pid){
3032       p->killed = 1;
3033       // Wake process from sleep if necessary.
3034       if(p->state == SLEEPING)
3035         p->state = RUNNABLE;
3036       release(&ptable.lock);
3037       return 0;
3038     }
3039   }
3040   release(&ptable.lock);
3041   return -1;
3042 }
3043 
3044 
3045 
3046 
3047 
3048 
3049 
3050 // Print a process listing to console.  For debugging.
3051 // Runs when user types ^P on console.
3052 // No lock to avoid wedging a stuck machine further.
3053 void
3054 procdump(void)
3055 {
3056   static char *states[] = {
3057   [UNUSED]    "unused",
3058   [EMBRYO]    "embryo",
3059   [SLEEPING]  "sleep ",
3060   [RUNNABLE]  "runble",
3061   [RUNNING]   "run   ",
3062   [ZOMBIE]    "zombie"
3063   };
3064   int i;
3065   struct proc *p;
3066   char *state;
3067   uint pc[10];
3068 
3069   for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
3070     if(p->state == UNUSED)
3071       continue;
3072     if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
3073       state = states[p->state];
3074     else
3075       state = "???";
3076     cprintf("%d %s %s", p->pid, state, p->name);
3077     if(p->state == SLEEPING){
3078       getcallerpcs((uint*)p->context->ebp+2, pc);
3079       for(i=0; i<10 && pc[i] != 0; i++)
3080         cprintf(" %p", pc[i]);
3081     }
3082     cprintf("\n");
3083   }
3084 }
3085 
3086 
3087 
3088 
3089 
3090 
3091 
3092 
3093 
3094 
3095 
3096 
3097 
3098 
3099 
