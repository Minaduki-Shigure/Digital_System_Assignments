# My FAT12 Operator

>部分关于文件系统结构的头文件(lib目录中)来自[OpenBSD项目](https://github.com/openbsd/src/tree/master/sys/msdosfs)。

## How to build
使用`make`命令生成可执行文件myls与mycp，使用`make clean`命令清理

## How to use 
* `./myls <image file>` 可以列出镜像上的所有文件
* `./mycp <image file> a:<source> <destination>` 可以从镜像上复制文件到你的文件系统中。
* `./mycp <image file> <source> a:<destination>` 可以从你的文件系统中复制文件到镜像上。