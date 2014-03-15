---
layout: post
title: "一段代码的重构"
date: 2014-03-10 07:26:03 +0800
comments: true
categories: code
keywords: code refactoring,experience, 代码重构,经验
description: 一段代码重构的经验
---


### 为什么要对这段代码重构


上周对一段代码进行了重构，这段代码是参加一个小组的code review时读到的，这段代码并不长，只有100多行，我花了几个小时（也许两个小时），但是没有读明白这段代码是如何工作的。因为代码可读性不好，代码阅读者的时间就这样被浪费了而没有任何收获。而且代码的书写者在一段时间后也会面临同样的问题，他／她至少需要一点时间才能明白代码如何工作的。还有很重要的一点是，从长远来看，这样的代码可维护性也不高，引入新的改动会比较难或者比较容易引入新的bug。为了避免下一个代码阅读者掉入同样的一个“坑”，我提出需要对代码重构。


### 为什么可读性不高


原始代码请看后面，它的可读性不好主要体现在以下几点：
 
- 变量的命名可读性。比如dummy命名为isdummy，peek_dummy命名为next_msg_isdummy的可读性会更好，sleep_entry实际上就是!q->msg_queue_pending_msgs变量有重复之嫌。关于如何命名可以参考[clean code][2]。 
- do ... while的循环条件是一个长条件(((*head == NULL) && !sleep_entry) || (dummy && sleep_entry) || unlikely(peek_dummy))，难读懂，这是造成可读性不好的主要原因，其实际体现出来的是代码的逻辑（或者流程）不清晰
- 函数的长度很长。并不是说长函数的可读性一定有问题，但是长函数往往是有问题，它通常反映代码书写者对代码没有进行很好的抽象。抽象是书写代码最终要的一种能力，如果我们打开备受[Peter Norvig][3]推崇的[Structure and Interpretation of Computer Programs][1]一书，它的第一章就是Building Abstractions with Procedures
- 注释太少，一些需要注释的而没有，例如上面提到的do ... while循环的三个条件

通过与拥有这段代码的小组讨论之后，代码的意图实际上如下面的流程图所示：

{% img media/a-piece-of-code-refactoring/flowchart.png %}

流程图没有经过简化，有一部分模块是重复的，但是可以让我们理解原代码的意图是什么。可以看到原代码将整个流程都在一个do ... while中实现了，这就导致代码结构不清晰。


### 代码重构中遵循的一些原则
重构后的代码请看后面，那么重构中遵循了哪些原则呢？并不复杂，只要写代码过程中，留意下面几条原则，代码的可读性就可以得到极大的提高。

- 给变量和函数一个可读性好的名字，正如我们在clean code中学到的
- 给if, when, switch等中的逻辑条件（condition）一个体现意图的可读性强的名字，这个condition可以是变量判断或者函数返回值实现的。
- 保持函数在一个合理的长度
- 在必要的时候画流程图可以解决混乱，注意这里流程图画的是我们希望代码做什么，而不是我们已经写的代码的流程图，这点很重要，因为我们要写出是执行我们意图的代码
- DRY principle，即不要在不同的代码重复相同的代码和信息
- 就象写完文章之前最后再把文章读一边，在提交代码前把代码再读一边，通常我们都可以发现一些改进的点。不要担心这会导致无止无=尽的修改，最终我们会到达一个我们能接受的点，另外deadline也会迫使我们在适当的时候停下。



### 重构之后的一些反思


- 我始终认为代码的可读性是最重要的，尤其是现在代码的开发都是团队合作的结果。虽然重构后的代码有将近200多行，这是因为有更多的函数，注释和空格。但新的代码可读性更好，长远看更容易维护（意味着成本更低）
- 写可读性强的代码需要时间，所以在计划和估计任务时请考虑这点
- 可读性好的代码不是一蹴而就的，好的代码通常是通过反复的、慢慢的改进而来，我想这就是为什么需要经常性的代码重构的思想的来源
- 可读性好的代码让读代码的人获得乐趣，写代码的人也会因为写了一份好的代码而获得乐趣
- 也许一开始我们写可读性好的代码会有难度或者觉得花费时间，但是只要坚持上述提到的原则，早晚我们可以又快又好的写出漂亮的代码
- 保持stupid simple，就象功夫，花巧并不一定好，真正的高手只用基础招数的组合
- 没有UT也可以重构，也不影响写出一个好的代码，但是看下一条
- 当代码提交后，新代码review之后，碰到的一个问题是：是否应该将新代码提交，这样的犹豫是因为没有足够的UT和funtional测试。完全可以理解，所以这就是为什么需要UT和CI。当然我这里也有个疑惑就是Linux Kernel开发没有UT，那么在Linuxe Kernel的演化过程中，它的开发人员是如何获得这样的信心的


### 重构前的代码


{% include_code [原始代码] lang:c a-piece-of-code-refactoring/recv_whole_msg_origin.c %}


### 重构后的代码


{% include_code [重构后的代码] lang:c a-piece-of-code-refactoring/recv_whole_msg_r2.c %}


  [1]: http://mitpress.mit.edu/sicp/full-text/book/book.html "Structure and Interpretation of Computer Programs"
  [2]: http://www.amazon.com/Clean-Code-Handbook-Software-Craftsmanship/dp/0132350882/ref=sr_1_1?ie=UTF8&qid=1394416932&sr=8-1&keywords=clean+code "Clean Code: A Handbook of Agile Software Craftsmanship"
  [3]: http://norvig.com/ "Peter Norvig"