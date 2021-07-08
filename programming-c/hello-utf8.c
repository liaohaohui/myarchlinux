/* 
   There's currently no free compiler to compile C program with utf-8 
   identifier.

   However, GCC-4.4 support C99/C++ extended-identifiers.
    (1) Need to convert utf-8 identifiers to unicode-escape as follows:
        > f = open("hello-utf8.c","r")
        > for i in f:
        >    i = i.strip()
        >    print(i.encode("unicode-escape").decode("ascii"))
    (2) /opt/gcc44/bin/gcc -std=c99 -fextended-identifiers -finput-charset=UTF-8 -fexec-charset=UTF-8 hello-enc.c
*/


#include <stdio.h>

void 显示(char *参数)
{
  printf("%s\n", 参数);
}

int main()
{
  显示("你好");
}

