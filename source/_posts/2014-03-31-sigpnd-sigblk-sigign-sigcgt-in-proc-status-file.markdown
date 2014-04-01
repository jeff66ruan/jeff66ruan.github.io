---
layout: post
title: "SigPnd, ShdPnd, SigBlk, SigIgn and SigCgt fields in proc information file system"
date: 2014-03-31 20:59:51 +0800
comments: true
categories: Linux
keywords: Linux, signal, proc, SigPnd, ShdPnd, SigBlk, SigCgt, status, file
description: The description of how to interpret SigPnd, ShdPnd, SigBlk and SigCgt of status file of proc information file system
---

The following is the field definition from Linux man proc(5). 

> SigPnd, ShdPnd: Number of signals pending for thread and for process as a whole (see pthreads(7) and signal(7)).
> 
> SigBlk, SigIgn, SigCgt: Masks indicating signals being blocked, ignored, and caught (see signal(7)).

But what they look like and how to interpret? Here is an output example:

> SigPnd: 0000000000000000
> ShdPnd: 0000000000000000
> SigBlk: fffffffe7ffbfeff
> SigIgn: 0000000000000000
> SigCgt: 0000000000000000

They are displayed in hex format as 8 bytes. The right most 4 bytes represent stardard signals, the left is Linux real-time signal extension. Each bit in the 8 bytes represents one corresponding signal. If the bits index starts from zero(the right most bit in the above), The corresponding signal is represented by bit[signalValue-1]. An example is that the signal SIGHUP, whose value is 1, is represented the bit 0.



Take the above SigBlk as example, the first two bytes are
> 0xfeff 

The binary format is  

> 1111,1110,1111,1111 

It means all signals from 1 to 16 are blocked except the signal 9 (SIGKILL). This is true because SIGKILL cannot be blocked or ignored.
