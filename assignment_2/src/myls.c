#include "../inc/myls.h"

int main(int argc, char** argv)
{    
	int fd;
    uint8_t *image_buf;
    struct bpb33* bpb;

    if (argc < 2 || argc > 2)
	{
		usage();
    }

    image_buf = mmap_file(argv[1], &fd);
    bpb = check_bootsector(image_buf);
    follow_dir(0, 0, image_buf, bpb);
    close(fd);
    exit(0);
}


/* 如果输入的argument格式不对，则提示使用方法 */
void usage(void)
{
    fprintf(stderr, "Usage: myls <imagename>\n");

    exit(1);
}

/* 输出n个字符长度的缩进 */
void print_indent_n(int indent)
{
	int i;
	for (i = 0; i < indent; i++)
	{
		printf(" ");
	}
	return;
}

void follow_dir(uint16_t cluster, int indent, uint8_t *image_buf, struct bpb33* bpb)
{
	int d, i;
    struct direntry *dirent;

    dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);
    
	while (1)
	{
		for (d = 0; d < bpb->bpbBytesPerSec * bpb->bpbSecPerClust; d += sizeof(struct direntry))
		{
			char name[9];
			char extension[4];
			uint32_t size;
			uint16_t file_cluster;
			name[8] = ' ';
			extension[3] = ' ';
			memcpy(name, &(dirent->deName[0]), 8);
			memcpy(extension, dirent->deExtension, 3);

			/* 如果slot是全新的，就直接返回 */
			if (((uint8_t)name[0]) == SLOT_EMPTY)
			{
				return;
			}

			/* 如果slot的文件对象已经被删除了，就跳过不管 */
			if (((uint8_t)name[0]) == SLOT_DELETED)
			{
				continue;
			}

			/* 对于用空格补全的名字，把空格删了改成EOF */
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

			/* 如果文件名为.(本目录)或..(上级目录)，跳过 */
			if (strcmp(name, ".") == 0)
			{
				dirent++;
				continue;
			}
			if (strcmp(name, "..") == 0)
			{
				dirent++;
				continue;
			}

			/* 卷级别 */
			if ((dirent->deAttributes & ATTR_VOLUME) != 0)
			{
				printf("Volume: %s\n", name);
			}
			/* 目录级别 */
			else if ((dirent->deAttributes & ATTR_DIRECTORY) != 0)
			{
				print_indent_n(indent);
				printf("%s (directory)\n", name);
				file_cluster = getushort(dirent->deStartCluster);
				/* 递归查找 */
				follow_dir(file_cluster, indent+2, image_buf, bpb);
			}
			/* 文件级别 */
			else
			{
				size = getulong(dirent->deFileSize);
				print_indent_n(indent);
				printf("%s.%s (%u bytes)\n", name, extension, size);
			}
			dirent++;
		}

		if (cluster == 0)
		{
			/* 根目录不管 */
			dirent++;
		}
		else
		{
			cluster = get_fat_entry(cluster, image_buf, bpb);
			dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);
		}
    }
}