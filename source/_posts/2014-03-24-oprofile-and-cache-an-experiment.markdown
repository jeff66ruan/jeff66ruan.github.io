---
layout: post
title: "Oprofile and Memory cache - an experiment"
date: 2014-03-24 11:03:38 +0800
comments: true
categories: code, Linux
keywords: cache, Linux, oprofile, memory, performance, profile
description: This is an experiment to show how to use oprofile profiling memory cache and performance of a piece of code
---


Last week, I did a small experiment of using oprofile to profile two piece of code. In this small experiment, I learned some basic ideas of using oprofile. In particular, I used oprofile to profile memory cache behaviour and the performance effect. In this experiment, I got ideas of how memory cache invalidation can affect the speed of running program. The following is the detail.


### The Experiment Configuration


I ran the experiment in an Thinkpad T400 with a Ubuntu installation. The experiment needs to run on a multi-core processor because of profiling memory cache invalidation cost. In a single core processor, there is no such memory cache invalidation cost.

```
$sudo lsb_release -a
No LSB modules are available.
Distributor ID: Ubuntu
Description:    Ubuntu 12.04 LTS
Release:        12.04
Codename:       precise

$ cat /proc/cpuinfo |grep "model name"
model name      : Intel(R) Core(TM)2 Duo CPU     P8600  @ 2.40GHz
model name      : Intel(R) Core(TM)2 Duo CPU     P8600  @ 2.40GHz
```

### Source Code


There are two pieces of programs, which are "no_alignment" and "alignment". They are compiled with GNU GCC. Each program clones a child sharing the same memory address space, which means the parent and the child updating different fileds of the same global data. The difference is that the program "alignment" optimizing the fields of the shared_data to the cache line size alignment. In this way, the two fields are able to be fetched on different cache lines. Therefore, when the parent and the child runs on different cores, the "no_alignment" program has the cache invlidation costs between two cores, but the "alignment" program doesn't. 

The following are the source codes of the two programs. The only difference is the definition of struct shared_data.

``` c no_alignment
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

// shared data
struct shared_data {
  unsigned int num_proc1;
  unsigned int num_proc2;
};

struct shared_data shared;

// define loops to run for a while
int loop_i = 100000, loop_j = 100000;

int child(void *);

#define STACK_SIZE (8 * 1024)

int main(void)
{
  pid_t pid;
  int i, j;

  /* Stack */
  char *stack = (char *)malloc(STACK_SIZE);
  if (!stack) {
    perror("malloc");
    exit(1);
  }

  printf("main: shared %p %p\n",
         &shared.num_proc1, &shared.num_proc2);

  /* clone a thread sharing memory space with the parent process */
  if ((pid = clone(child, stack + STACK_SIZE, CLONE_VM, NULL)) < 0) {
    perror("clone");
    exit(1);
  }

  for (i = 0; i < loop_i; i++) {
    for(j = 0; j < loop_j; j++) {
      shared.num_proc1++;
    }
  }
}

int child(void *arg)
{
  int i, j;

  printf("child: shared %p %p\n",
         &shared.num_proc1, &shared.num_proc2);

  for (i = 0; i < loop_i; i++) {
    for (j = 0; j < loop_j; j++) {
      shared.num_proc2++;
    }
  }
}
```

``` c alignment
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

// cache line size
// hardware dependent value
// it can checks from /proc/cpuinfo
#define CACHE_LINE_SIZE 64

// shared data aligned with cache line size
struct shared_data {
  unsigned int __attribute__ ((aligned (CACHE_LINE_SIZE))) num_proc1;
  unsigned int __attribute__ ((aligned (CACHE_LINE_SIZE))) num_proc2;
};

struct shared_data shared;

// define loops to run for a while
int loop_i = 100000, loop_j = 100000;

int child(void *);

#define STACK_SIZE (8 * 1024)

int main(void)
{
  pid_t pid;
  int i, j;

  /* Stack */
  char *stack = (char *)malloc(STACK_SIZE);
  if (!stack) {
    perror("malloc");
    exit(1);
  }

  printf("main: shared %p %p\n",
         &shared.num_proc1, &shared.num_proc2);

  /* clone a thread sharing memory space with the parent process */
  if ((pid = clone(child, stack + STACK_SIZE, CLONE_VM, NULL)) < 0) {
    perror("clone");
    exit(1);
  }

  for (i = 0; i < loop_i; i++) {
    for(j = 0; j < loop_j; j++) {
      shared.num_proc1++;
    }
  }
}

int child(void *arg)
{
  int i, j;

  printf("child: shared %p %p\n",
         &shared.num_proc1, &shared.num_proc2);

  for (i = 0; i < loop_i; i++) {
    for (j = 0; j < loop_j; j++) {
      shared.num_proc2++;
    }
  }
}

```


### Testing

I was using the Oprofile 0.99 which has 'operf' program that allows non-root users to profile a specified individual process with less setup. Different CPUs have different supported events. The supported events, their meanings and accepted event format by operf can be checked from ophelp and the CPU's hardware manual. In this experiment, the CLOCK event is CPU_CLK_UNHALTED and L2_LINES_IN is the number of allocated lines in L2 which is L2 cache missing number. As examples, the "CPU_CLK_UNHALTED:100000" tells operf to sample every 100000 unhalted clock with default mask(unhalted core cycles). The "L2_LINES_IN:100000:0xf0" tells operf to sample every 1000000 number of allocated lines in L2 on all cores with all prefetch inclusive.

The following is the experiment results.

``` bash profiling result of alignment
$time operf --event=CPU_CLK_UNHALTED:100000,L2_LINES_IN:100000:0xf0 ./alignment

operf: Profiler started
main: shared 0x6010c0 0x601100
child: shared 0x6010c0 0x601100

profiled app exited with the following status: 160

Profiling done.

real    0m41.373s
user    0m54.015s
sys     0m0.728s


$opreport
Using /home/work/oprof_cache/oprofile_data/samples/ for samples directory.
CPU: Core 2, speed 2.401e+06 MHz (estimated)
Counted CPU_CLK_UNHALTED events (Clock cycles when not halted) with a unit mask of 0x00 (Unhalted core cycles) count 100000
Counted L2_LINES_IN events (number of allocated lines in L2) with a unit mask of 0xf0 (multiple flags) count 100000
CPU_CLK_UNHALT...|L2_LINES_IN:10...|
  samples|      %|  samples|      %|
------------------------------------
   939093 100.000    396933 100.000 alignment
        CPU_CLK_UNHALT...|L2_LINES_IN:10...|
          samples|      %|  samples|      %|
        ------------------------------------
           936430 99.7164    395920 99.7448 alignment
             2657  0.2829      1013  0.2552 no-vmlinux
                5 5.3e-04         0       0 ld-2.15.so
                1 1.1e-04         0       0 libc-2.15.so
```


``` bash profiling result of no_alignment
$time operf --event=CPU_CLK_UNHALTED:100000,L2_LINES_IN:100000:0xf0 ./no_alignment
operf: Profiler started
main: shared 0x601058 0x60105c
child: shared 0x601058 0x60105c
profiled app exited with the following status: 160

Profiling done.

real    1m24.739s
user    1m59.167s
sys     0m1.344s

$opreport
Using /home/work/oprof_cache/oprofile_data/samples/ for samples directory.
CPU: Core 2, speed 2.401e+06 MHz (estimated)
Counted CPU_CLK_UNHALTED events (Clock cycles when not halted) with a unit mask of 0x00 (Unhalted core cycles) count 100000
Counted L2_LINES_IN events (number of allocated lines in L2) with a unit mask of 0xf0 (multiple flags) count 100000
CPU_CLK_UNHALT...|L2_LINES_IN:10...|
  samples|      %|  samples|      %|
------------------------------------
  1423030 100.000   1177262 100.000 no_alignment
        CPU_CLK_UNHALT...|L2_LINES_IN:10...|
          samples|      %|  samples|      %|
        ------------------------------------
          1419249 99.7343   1174038 99.7261 no_alignment
             3778  0.2655      3224  0.2739 no-vmlinux
                2 1.4e-04         0       0 ld-2.15.so
                1 7.0e-05         0       0 libc-2.15.so
```


### Analysis and Conclusion

I drawed the table for the analysising.


{% img media/Oprofile-and-Cache/Oprofile-and-Cache.png %}


Here is the conclusion:

- The cache missing rate of no_alignment was 82.73%. It became 42.27% with alignment optimization. The cache missing rate was reduced to almost half.
- The no_alignment ran 120.5 seconds and the alignment ran 55 seconds. It was a bit more than double faster with alignment optimization.


> **NOTE:** The real time is less than user + sys because the program runs on mutlecores parallel