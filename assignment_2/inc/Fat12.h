#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "../lib/bootsect.h"
#include "../lib/bpb.h"
#include "../lib/direntry.h"
#include "../lib/fat.h"

#define MAXPATHLEN 255

#ifndef TRUE
#define TRUE (1)
#define FALSE (0)
#endif

uint8_t *mmap_file(char *filename, int *fd);
struct bpb33* check_bootsector(uint8_t *image_buf);
uint16_t get_fat_entry(uint16_t clusternum, uint8_t *image_buf, 
		       struct bpb33* bpb);
void set_fat_entry(uint16_t clusternum, uint16_t value, 
		   uint8_t *image_buf, struct bpb33* bpb);
int is_end_of_file(uint16_t cluster) ;
uint8_t *root_dir_addr(uint8_t *image_buf, struct bpb33* bpb);
uint8_t *cluster_to_addr(uint16_t cluster, uint8_t *image_buf, 
			 struct bpb33* bpb);