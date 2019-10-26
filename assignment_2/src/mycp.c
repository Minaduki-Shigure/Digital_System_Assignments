#include "../inc/mycp.h"

int main(int argc, char** argv)
{
    int fd;
    uint8_t *image_buf;
    struct bpb33* bpb;

    if (argc < 4 || argc > 4)
    {
	    usage();
    }

    image_buf = mmap_file(argv[1], &fd);
    bpb = check_bootsector(image_buf);

    /* 将a:当作虚拟的卷标 */
    if (strncmp("a:", argv[2], 2)==0)
    {
        /* 读取镜像文件 */
        copyout(argv[2], argv[3], image_buf, bpb);
    }
    else if (strncmp("a:", argv[3], 2)==0)
    {
        /* 写入镜像文件 */
        copyin(argv[2], argv[3], image_buf, bpb);
    }
    else
    {
	    usage();
    }

    close(fd);
    exit(0);
}


/* 如果输入的argument格式不对，则提示使用方法 */
void usage(void)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "To copy a file called src from disk image to a normal file as dst, type:\n");
    fprintf(stderr, "  mycp <imagename> a:<src> <dst>\n");
    fprintf(stderr, "To copy a normal file called src into disk image as dst, type:\n");
    fprintf(stderr, "  mycp <imagename> <src> a:<dst>\n");

    exit(1);
}

/* 获取文件名 */
void get_name(char *fullname, struct direntry *dirent) 
{
    char name[9];
    char extension[4];
    int i;

    name[8] = ' ';
    extension[3] = ' ';
    memcpy(name, &(dirent->deName[0]), 8);
    memcpy(extension, dirent->deExtension, 3);

    /* 对于用空格补全的名字，把空格删了改成EOF*/
    for (i = 8; i > 0; --i)
    {
        if (name[i] == ' ')
        {
            name[i] = '\0';
        }
        else
        {
            break;
        }
    }

    /* 拓展名也同样这样删掉补位空格 */
    for (i = 3; i > 0; --i)
    {
        if (extension[i] == ' ')
        {
            extension[i] = '\0';
        }
        else
        {
            break;
        }
    }

    fullname[0]='\0';
    strcat(fullname, name);

    /* 如果是目录就不管，否则加上拓展名 */
    if ((dirent->deAttributes & ATTR_DIRECTORY) == 0)
    {
        strcat(fullname, ".");
        strcat(fullname, extension);
    }
}

struct direntry* find_file(char *infilename, uint16_t cluster, int find_mode, uint8_t *image_buf, struct bpb33* bpb)
{
    int d;
    char buf[MAXPATHLEN];
    char *seek_name, *next_name;
    struct direntry *dirent;
    uint16_t dir_cluster;
    char fullname[13];

    /* 打开目录 */
    dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);

    /* 接下来，将文件的路径拆成目录+文件名的格式 */
    strncpy(buf, infilename, MAXPATHLEN);
    seek_name = buf;

    /* 舍去第一个斜杠 */
    while (*seek_name == '/' || *seek_name == '\\')
    {
	    seek_name++;
    }

    /* 继续搜索，如果又找到了斜杠，证明这是一个目录 */
    next_name = seek_name;
    while (1)
    {
        if (*next_name == '/' || *next_name == '\\')
        {
            *next_name = '\0';
            next_name ++;
            break;
        }
        if (*next_name == '\0')
        {
            /* 没有斜杠了，这是一个文件 */
            next_name = NULL;
            if (find_mode == FIND_DIR)
            {
                return dirent;
            }
            break;
        }
        next_name++;
    }

    while (1)
    {
        /* 寻找对应的簇，直到最后一个簇被找到 */
        for (d = 0; d < bpb->bpbBytesPerSec * bpb->bpbSecPerClust; d += sizeof(struct direntry))
        {
            if (dirent->deName[0] == SLOT_EMPTY)
            {
                /* slot是空的 */
                return NULL;
            }
            if (dirent->deName[0] == SLOT_DELETED)
            {
                /* slot处文件已被删除 */
                dirent++;
                continue;
            }

            get_name(fullname, dirent);
            if (strcmp(fullname, seek_name)==0)
            {
                /* 找到了目标 */
                if ((dirent->deAttributes & ATTR_DIRECTORY) != 0)
                {
                    /* 如果是一个目录 */
                    if (next_name == NULL)
                    {
                        fprintf(stderr, "Error: Cannot copy out a directory\n");
                        exit(1);
                    }
                    dir_cluster = getushort(dirent->deStartCluster);
                    /* 递归寻找 */
                    return find_file(next_name, dir_cluster, find_mode, image_buf, bpb);
                }
                else if ((dirent->deAttributes & ATTR_VOLUME) != 0)
                {
                    /* 如果是一个卷 */
                    fprintf(stderr, "Error: Cannot copy out a volume\n");
                    exit(1);
                }
                else
                {
                    /* 返回文件的dirent */
                    return dirent;
                }
            }
            dirent++;
        }
        /* 当前簇读完，寻找下一个 */
        if (cluster == 0)
        {
            /* 根目录直接往下走 */
            dirent++;
        }
        else
        {
            cluster = get_fat_entry(cluster, image_buf, bpb);
            dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);
        }
    }
}

/* 这个是实际干活的函数 */
/* 寻找文件对应的簇，挨个copy */
void copy_out_file(FILE *fd, uint16_t cluster, uint32_t bytes_remaining, uint8_t *image_buf, struct bpb33* bpb)
{
    int total_clusters, clust_size;
    uint8_t *p;

    clust_size = bpb->bpbSecPerClust * bpb->bpbBytesPerSec;
    total_clusters = bpb->bpbSectors / bpb->bpbSecPerClust;

    if (cluster == 0)
    {
        fprintf(stderr, "Bad file termination\n");
        return;
    }
    /* 已到达最后一个簇 */
    else if (is_end_of_file(cluster))
    {
	    return;	
    }
    /* 传入的簇号越界 */
    else if (cluster > total_clusters)
    {
	    abort();
    }

    /* 找到memory map中该簇对应的地址 */
    p = cluster_to_addr(cluster, image_buf, bpb);

    if (bytes_remaining <= clust_size)
    {
        /* 剩余待处理的数据大小不足一个簇大小，这是最后一个簇了，只写入剩余大小 */
        fwrite(p, bytes_remaining, 1, fd);
    }
    else
    {
        /* 后面还有，把整个簇写入文件 */
        fwrite(p, clust_size, 1, fd);

        /* 递归，copy下一个簇 */
        copy_out_file(fd, get_fat_entry(cluster, image_buf, bpb), bytes_remaining - clust_size, image_buf, bpb);
    }
    return;
}


/* 这个是寻找目标的函数 */
/* 找到src的dirent并且打开dst的socket */
void copyout(char *infilename, char* outfilename, uint8_t *image_buf, struct bpb33* bpb)
{
    FILE *fd;
    struct direntry *dirent = (void*)1;
    
    uint16_t start_cluster;
    uint32_t size;

    /* 舍去虚拟的卷标 */
    assert(strncmp("a:", infilename, 2)==0);
    infilename+=2;

    /* 找到文件对应的dirent */
    dirent = find_file(infilename, 0, FIND_FILE, image_buf, bpb);
    /* 文件不存在 */
    if (dirent == NULL)
    {
	    fprintf(stderr, "No file called %s exists in the disk image\n", infilename);
	    exit(1);
    }

    /* 打开写入目标文件 */
    fd = fopen(outfilename, "w");
    if (fd == NULL)
    {
	    fprintf(stderr, "Cannot open file %s to copy data out\nCheck if you have the permission to do so.\n", outfilename);
	    exit(1);
    }

    /* 调用逐簇递归copy */
    start_cluster = getushort(dirent->deStartCluster);
    size = getulong(dirent->deFileSize);
    copy_out_file(fd, start_cluster, size, image_buf, bpb);
    
    fclose(fd);
}

/* 写入部分的代码似乎不是总能成功 */
/* 这个是实际干活的函数 */
/* copy文件内容至镜像中，更新FAT表，返回值为起始簇的位置 */
uint16_t copy_in_file(FILE* fd, uint8_t *image_buf, struct bpb33* bpb, uint32_t *size)
{
    uint32_t clust_size, total_clusters, i;
    uint8_t *buf;
    size_t bytes;
    uint16_t start_cluster = 0;
    uint16_t prev_cluster = 0;
    
    clust_size = bpb->bpbSecPerClust * bpb->bpbBytesPerSec;
    total_clusters = bpb->bpbSectors / bpb->bpbSecPerClust;
    buf = malloc(clust_size);

    while(1)
    {
        /* 将一个簇大小的数据读入内存中 */
        bytes = fread(buf, 1, clust_size, fd);
        /* bytes==0就证明读完了 */
        if (bytes > 0)
        {
            /* 更新文件大小 */
            *size += bytes;

            /* 找一个没有被占用的簇 */
            for (i = 2; i < total_clusters; ++i)
            {
                if (get_fat_entry(i, image_buf, bpb) == CLUST_FREE)
                {
                    break;
                }
            }

            if (i == total_clusters)
            {
                /* 找到了最后一个簇都不是free的，证明磁盘空间用完了 */
                fprintf(stderr, "Error: Space run out in the disk\n");
                free(buf);
                exit(1);
            }

            /* 如果起始簇的位置还没有被记录，那就记录下来 */
            if (start_cluster == 0)
            {
                start_cluster = i;
            }
            else
            {
                /* 在FAT表中把前一个簇和当前簇连接起来 */
                assert(prev_cluster != 0);
                set_fat_entry(prev_cluster, i, image_buf, bpb);
            }

            /* 把当前簇设为已被占用 */
            set_fat_entry(i, FAT12_MASK&CLUST_EOFS, image_buf, bpb);

            /* 将数据写入镜像映射到的簇中 */
            memcpy(cluster_to_addr(i, image_buf, bpb), buf, clust_size);
        }
        if (bytes < clust_size)
        {
            /* 文件写完了，这是最后一个簇 */
            break;
        }
        prev_cluster = i;
    }

    free(buf);
    return start_cluster;
}

/* 这个是实际干活的函数 */
/* 修改目录项dirent的内容 */
void write_dirent(struct direntry *dirent, char *filename, uint16_t start_cluster, uint32_t size)
{
    char *p, *p2;
    char *uppername;
    int len, i;

    /* 清空原有的内容 */
    memset(dirent, 0, sizeof(struct direntry));

    /* 复制文件名 */
    uppername = strdup(filename);
    p2 = uppername;
    for (i = 0; i < strlen(filename); ++i)
    {
        if (p2[i] == '/' || p2[i] == '\\')
        {
            uppername = p2 + i + 1;
	    }
    }

    /* 转换成大写字母 */
    for (i = 0; i < strlen(uppername); ++i)
    {
	    uppername[i] = toupper(uppername[i]);
    }

    /* 将文件名和拓展名写入 */
    memset(dirent->deName, ' ', 8);
    p = strchr(uppername, '.');
    memcpy(dirent->deExtension, "___", 3);
    if (p == NULL)
    {
	    fprintf(stderr, "No filename extension given - defaulting to .___\n");
    }
    else
    {
        *p = '\0';
        p++;
        len = strlen(p);
        /* 舍去过长的拓展名部分 */
        if (len > 3)
        {
            len = 3;
        }
        memcpy(dirent->deExtension, p, len);
    }
    if (strlen(uppername)>8)
    {
	    /* 舍去过长的文件名 */
        uppername[8]='\0';
    }
    memcpy(dirent->deName, uppername, strlen(uppername));
    free(p2);

    /* 修改文件的大小与属性(不过偷懒没写入时间)*/
    dirent->deAttributes = ATTR_NORMAL;
    putushort(dirent->deStartCluster, start_cluster);
    putulong(dirent->deFileSize, size);

    return;
}

/* 这个是实际干活的函数 */
/* 在dirent中找一个没有被占用的slot，准备写入 */
void create_dirent(struct direntry *dirent, char *filename, uint16_t start_cluster, uint32_t size, uint8_t *image_buf, struct bpb33* bpb)
{
    while(1)
    {
        if (dirent->deName[0] == SLOT_EMPTY)
        {
            /* 找到了想要的空slot */
            write_dirent(dirent, filename, start_cluster, size);
            dirent++;

            /* 确保下个slot也被设为空的 */
            memset((uint8_t*)dirent, 0, sizeof(struct direntry));
            dirent->deName[0] = SLOT_EMPTY;
            return;
        }
        if (dirent->deName[0] == SLOT_DELETED)
        {
            /* 或者找到了一个原文件被删除的slot */
            write_dirent(dirent, filename, start_cluster, size);
            return;
        }
        dirent++;
    }
}

/* 这个是寻找目标的函数 */
/* 打开src的socket并且打开dst的socket */
void copyin(char *infilename, char* outfilename, uint8_t *image_buf, struct bpb33* bpb)
{
    FILE *fd;
    struct direntry *dirent = (void*)1;
    uint16_t start_cluster;
    uint32_t size = 0;

    /* 检查格式是否正确，舍去虚拟卷标 */
    assert(strncmp("a:", outfilename, 2) == 0);
    outfilename += 2;

    /* 检查一下文件名是否重复 */
    dirent = find_file(outfilename, 0, FIND_FILE, image_buf, bpb);
    if (dirent != NULL)
    {
        fprintf(stderr, "Error: File %s already exists\n", outfilename);
        exit(1);
    }

    /* 找到想要存放文件的目录 */
    dirent = find_file(outfilename, 0, FIND_DIR, image_buf, bpb);
    if (dirent == NULL)
    {
        fprintf(stderr, "Error: Directory does not exists in the disk image\n");
        exit(1);
    }

    /* 打开源文件 */
    fd = fopen(infilename, "r");
    if (fd == NULL)
    {
        fprintf(stderr, "Error: Cannot open file %s to copy data in\nCheck if you have the permission to do so.\n", infilename);
        exit(1);
    }

    /* 调用写入簇的函数 */
    start_cluster = copy_in_file(fd, image_buf, bpb, &size);

    /* 调用更新dirent的函数 */
    create_dirent(dirent, outfilename, start_cluster, size, image_buf, bpb);
    
    fclose(fd);
}