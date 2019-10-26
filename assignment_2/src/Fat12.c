#include "../inc/Fat12.h"

/* 生成磁盘镜像的memory map */
uint8_t *mmap_file(char *filename, int *fd)
{
    struct stat statbuf;
    int size;
    uint8_t *image_buf;
    char pathname[MAXPATHLEN + 1];

    /* 如果地址不是绝对路径，就加上当前路径，调用系统函数实现当前路径的获取 */
    if (filename[0] == '/')
    {
	    strncpy(pathname, filename, MAXPATHLEN);
    }
    else
    {
	    getcwd(pathname, MAXPATHLEN);
        /* 如果路径太长，报错退出 */
	    if (strlen(pathname) + strlen(filename) + 1 > MAXPATHLEN)
        {
	        fprintf(stderr, "Filepath exceeded the limit, consider to define a larger size while compiling.\n");
	        exit(1);
	    }
        strcat(pathname, "/");
        strcat(pathname, filename);
    }

    /* 调用系统函数，查看镜像的大小 */
    if (stat(pathname, &statbuf) < 0)
    {
	    fprintf(stderr, "Failed to fetch the stats of the disk image file %s:\n%s\n", pathname, strerror(errno));
	    exit(1);
    }

    size = statbuf.st_size;

    /* 使用open函数打开文件，并且把fd传出 */
    *fd = open(pathname, O_RDWR);
    if (*fd < 0)
    {
	    fprintf(stderr, "Failed to open the disk image file %s:\n%s\nCheck if you have enough permission to do so.\n", pathname, strerror(errno));
	    exit(1);
    }

    /* 调用系统函数，生成memory map */
    image_buf = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
    if (image_buf == MAP_FAILED)
    {
	    fprintf(stderr, "Failed to memory map: \n%s\n", strerror(errno));
	    exit(1);
    }
    return image_buf;
}

/* 读取磁盘镜像的bootsector，检查其是否是标准的FAT12镜像 */
struct bpb33* check_bootsector(uint8_t *image_buf)
{
    struct bootsector33* bootsect;
    struct byte_bpb33* bpb;  /* BIOS parameter block */
    struct bpb33* bpb2;

    bootsect = (struct bootsector33*)image_buf;
    if (bootsect->bsJump[0] == 0xe9 ||
	(bootsect->bsJump[0] == 0xeb && bootsect->bsJump[2] == 0x90))
    {
        //fprintf(stderr, "Good\n");
    }
    else
    {
	    fprintf(stderr, "Illegal boot sector jump inst: %x%x%x\nCheck if the image is corrupted.\n", bootsect->bsJump[0], bootsect->bsJump[1], bootsect->bsJump[2]); 
    } 

    if (bootsect->bsBootSectSig0 == BOOTSIG0 && bootsect->bsBootSectSig0 == BOOTSIG0)
    {	
	    //fprintf(stderr, "Good\n");
    }
    else
    {
	    fprintf(stderr, "Bad boot sector signature %x%x\nCheck if the image is corrupted.\n", bootsect->bsBootSectSig0, bootsect->bsBootSectSig1);
    }

    bpb = (struct byte_bpb33*)&(bootsect->bsBPB[0]);

    /* 按照字节对齐 */
    bpb2 = malloc(sizeof(struct bpb33));

    bpb2->bpbBytesPerSec = getushort(bpb->bpbBytesPerSec);
    bpb2->bpbSecPerClust = bpb->bpbSecPerClust;
    bpb2->bpbResSectors = getushort(bpb->bpbResSectors);
    bpb2->bpbFATs = bpb->bpbFATs;
    bpb2->bpbRootDirEnts = getushort(bpb->bpbRootDirEnts);
    bpb2->bpbSectors = getushort(bpb->bpbSectors);
    bpb2->bpbFATsecs = getushort(bpb->bpbFATsecs);
    bpb2->bpbHiddenSecs = getushort(bpb->bpbHiddenSecs);

    /* Debug information
    printf("Bytes per sector: %d\n", bpb2->bpbBytesPerSec);
    printf("Sectors per cluster: %d\n", bpb2->bpbSecPerClust);
    printf("Reserved sectors: %d\n", bpb2->bpbResSectors);
    printf("Number of FATs: %d\n", bpb->bpbFATs);
    printf("Number of root dir entries: %d\n", bpb2->bpbRootDirEnts);
    printf("Total number of sectors: %d\n", bpb2->bpbSectors);
    printf("Number of sectors per FAT: %d\n", bpb2->bpbFATsecs);
    printf("Number of hidden sectors: %d\n", bpb2->bpbHiddenSecs);
    */

    return bpb2;
}

/* 根据簇号返回内容 */
uint16_t get_fat_entry(uint16_t clusternum, uint8_t *image_buf, struct bpb33* bpb)
{
    uint32_t offset;
    uint16_t value;
    uint8_t b1, b2;
    
    /* 进行移位操作 */
    /* 如果机器不是little-endian，要先进行转换，这里没有写 */
    /* 本机环境：Ubuntu-18.04 x86_64 kernel: 4.4.0-18362-Microsoft*/
    offset = bpb->bpbResSectors * bpb->bpbBytesPerSec * bpb->bpbSecPerClust + (3 * (clusternum / 2));

    switch(clusternum % 2)
    {
        case 0:
            b1 = *(image_buf + offset);
            b2 = *(image_buf + offset + 1);
            value = ((0x0f & b2) << 8) | b1;
            break;
        case 1:
            b1 = *(image_buf + offset + 1);
            b2 = *(image_buf + offset + 2);
            value = b2 << 4 | ((0xf0 & b1) >> 4);
            break;
    }

    return value;
}

/* 根据簇号写入数据 */
void set_fat_entry(uint16_t clusternum, uint16_t value, uint8_t *image_buf, struct bpb33* bpb)
{
    uint32_t offset;
    uint8_t *p1, *p2;
    
    /* 进行移位操作 */
    /* 如果机器不是little-endian，要先进行转换，这里没有写 */
    offset = bpb->bpbResSectors * bpb->bpbBytesPerSec * bpb->bpbSecPerClust	+ (3 * (clusternum/2));

    switch(clusternum % 2)
    {
        case 0:
            p1 = image_buf + offset;
            p2 = image_buf + offset + 1;
            *p1 = (uint8_t)(0xff & value);
            *p2 = (uint8_t)((0xf0 & (*p2)) | (0x0f & (value >> 8)));
        break;
        case 1:
            p1 = image_buf + offset + 1;
            p2 = image_buf + offset + 2;
            *p1 = (uint8_t)((0x0f & (*p1)) | ((0x0f & value) << 4));
            *p2 = (uint8_t)(0xff & (value >> 4));
            break;
    }
}


/* 如果这是此文件最后一个簇，返回true，否则返回false */
int is_end_of_file(uint16_t cluster)
{
    if (cluster >= (FAT12_MASK & CLUST_EOFS) && cluster <= (FAT12_MASK & CLUST_EOFE))
    {
	    return TRUE;
    }
    else
    {
	    return FALSE;
    }
}


/* 返回memory map中根目录所在的地址 */
uint8_t *root_dir_addr(uint8_t *image_buf, struct bpb33* bpb)
{
    uint32_t offset;
    offset = (bpb->bpbBytesPerSec * (bpb->bpbResSectors + (bpb->bpbFATs * bpb->bpbFATsecs)));
    return image_buf + offset;
}

/* 返回memory map中指定的簇的起始地址 */
uint8_t *cluster_to_addr(uint16_t clusternum, uint8_t *image_buf, struct bpb33* bpb)
{
    uint8_t *p;
    p = root_dir_addr(image_buf, bpb);
    /* 如果所求的目录不是根目录 */
    if (clusternum != MSDOSFSROOT)
    {
        /* 找到根目录的终止点 */
        p += bpb->bpbRootDirEnts * sizeof(struct direntry);
        /* 继续前进到指定的簇 */
        p += bpb->bpbBytesPerSec * bpb->bpbSecPerClust * (clusternum - CLUST_FIRST);
    }
    return p;
}