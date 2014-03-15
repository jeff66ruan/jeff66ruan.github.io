---
layout: post
title: "双向链表在Linux Kernel中的实现"
date: 2014-02-27 05:41:58 +0800
comments: true
categories: [Linux,code]
keywords: Linux,data structure, doubly linked list, list_head, list.h, implementation,数据结构,双向链表,实现
description: Linux双向链表的笔记
---


双向链表是一个简单的数据结构,使用非常广泛，应该人人知道，会用，打开编辑器马上可以写一个。但是Linux的双向链表实现非常巧妙，它使用了一个小小的编程技巧，使得开发人员定义自己的数据结构时添加一个struct list_head字段（list_head是Linux Kernel定义的双向链表的数据结构），就可以使自己的数据结构具备双向链表功能，采用Linux Kernel提供的操作函数，使得使用双向链表非常简单。如果我们在v3.9-rc8中grep list_head，竟然有多达11400多条匹配，可见双向链表在Linux kernel中的江湖地位，实际上，我们在读Linux源码时，到处可以见到以list_head组织起来的数据结构。

{% img media/Doubly-Linked-List-in-the-Linux-Kernel/lines-of-list-head.png %}

下面我们将详细看看list_head结构及其相关函数，并且用一个简单例子学习如何使用它们。


#### list_head结构


数据结构是代码的核心，我们先来看看双向链表的数据结构。linux kernel在<linux/list.h>中定义了双向链表的数据结构struct list_head。list_head定义很简单，只有两个成员函数next和prev，用于指向list_head：

``` c struct list_head的定义
struct list_head {
       struct list_head *next, *prev;
};
```

#### 函数列表


在list_head定义的基础上，Linux kernel提供了丰富的链表操作函数，下表列出了其中一些主要的。所有双向链表的操作都应该使用操作函数，而不应该直接访问list_head内部的next和prev指针，这样当list_head的实现改变时，使用的代码可以不受到影响。

{% img media/Doubly-Linked-List-in-the-Linux-Kernel/list-operations.png %}


#### 使用


Linux kernel的双向列表实现巧妙，只要在我们的数据结构中定义一个struct list_head的字段就可以使我们的数据结构具备双向链表的功能。通过链表提供的操作函数，我们可以创建自己的双向链表。一个创建好的链表为下图所示的组织形式：

{% img media/Doubly-Linked-List-in-the-Linux-Kernel/list-head-general.png %}

我们可以通过一个例子来具体的理解和体会这个结构的方便简单的使用方式。

#### 例子


##### 定义自己的数据结构


定义我们的struct my_struct_t结构：

``` c 定义my_struct_t
typedef struct my_struct {
  int id;
  struct list_head lists;
  char *name;
} my_struct_t;
```

struct list_head可以在数据结构中任意位置。上面代码中，我们定义了一个my_struct_t的结构，有三个字段id, lists和name。其中lists申明为struct list_head.

##### 定义链表头


``` c 定义链表头my_struct_list
LIST_HEAD(my_struct_list);
```

LIST_HEAD是一个宏，它实际上声明了一个struct list_head变量my_struct_list，并且初始化字段next和prev指向自身。初始化完成之后，my_struct_list实际为如下图所示：

{% img media/Doubly-Linked-List-in-the-Linux-Kernel/list-head-zero.png %}


##### 插入一个链表项


我们可以用list_add(new, head)向在链表头部插入一个链表项。


``` c 插入my_struct2到链表
my_struct_t my_struct2 = {.id = 2, 
                          .name = "struct2"};
list_add(&my_struct2.lists, &my_struct_list); 
```

插入my_struct2之后，我们看到的链表结构如下图所示：

{% img media/Doubly-Linked-List-in-the-Linux-Kernel/list-head-one.png %}


##### 再插入一个链表项


``` c 插入my_struct1到链表
my_struct_t my_struct1 = {.id = 1,
                          .name = "struct1"};
list_add(&my_struct1.lists, &my_struct_list); 
```

插入my_struct1之后，我们看到的链表结构如下图所示：

{% img media/Doubly-Linked-List-in-the-Linux-Kernel/list-head-two.png %}

##### 从链表项指针得到原数据结构的指针


从链表项指针到原数据结构的指针是双向链表使用便利的关键，如果我们得到了一个链表项，可以通过list_entry(ptr, type, member)来得到指向struct list_head嵌入的数据结构的指针。其中ptr是一个指向struct list_head的指针，type是struct list_head嵌入的数据结构，memeber是struct list_head嵌入的字段名。看一个例子会容易明白：

``` c 得到指向原数据结构的指针
my_struct_t *my_structp = list_entry(my_struct_list.next, my_struct_t, lists);
```

my_structp会指向上述链表结构中my_struct1，和下述赋值语句等效

``` c
my_structp = &my_struct1;
```

大家可能对list_entry如何实现比较好奇，下面是简化的源代码，使用了c中一个常见的技巧：

``` c list_entry的实现
#define list_entry(ptr, type, member) \
          (type *) ((char *) ptr - offset(type, memeber);

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
```

offsetof计算字段MEMBER在类型TYPE中的偏移，这个偏移在编译的时候就由编译器计算得到。list_entry将链表项的指针减去list_head字段的偏移就得到了原数据结构的指针。


##### 遍历整个链表list_for_each和list_for_each_entry


使用list_for_each(pos, head)可以遍历整个链表的链表项，head是链表头，pos是指向当前链表项的指针，每次循环时，pos都会更新为下一个链表项，下面例子打印列表中的id和name：


``` c 遍历一
struct list_head *pos;
my_struct_t *my_structp;

list_for_each(pos, &my_struct_list) {
  my_structp = list_entry(pos, my_struct_t, lists);
  printf("id=%d, name=%s\n", my_structp->id, my_structp->name);
}
```

输出
> id=1, name=struct1
> id=2, name=struct2


list_for_each_entry(pos, head, member)可以遍历整个链表的原数据结构，head是链表头，pos为指向当前链表项所嵌入的原数据结构的指针，member是被嵌入数据结构struct list_head申明的字段名。实际上是list_for_each和list_entry的组合，所以上段代码也可以写成下面的方式，看起来更加简洁：


``` c 遍历二
my_struct_t *my_structp;

list_for_each_entry(my_structp, &my_struct_list, lists) {
  printf("id=%d, name=%s\n", my_structp->id, my_structp-name);
}
```


#### 总结


1. 使用双向链表需要在自己的数据结构中申明一个struct list_head的字段，定义的数据结构通过这个字段链接起来
1. 使用LIST_HEAD宏申明一个链表头，这个链表头指向双向链表
1. 使用<linux/list.h>提供的add，delete等操作函数管理链表
1. 通过list_entry及其他_entry结尾的函数得到原数据结构的指针，list_entry的实现采用了一个小技巧，是链表方便易用的原因