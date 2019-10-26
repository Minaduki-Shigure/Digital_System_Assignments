#include "../inc/Fat12.h"

/* find_file seeks through the directories in the memory disk image,
   until it finds the named file */

/* flags, depending on whether we're searching for a file or a
   directory */
#define FIND_FILE 0
#define FIND_DIR 1

void usage(void);
void get_name(char *fullname, struct direntry *dirent);
struct direntry* find_file(char *infilename, uint16_t cluster,
			   int find_mode,
			   uint8_t *image_buf, struct bpb33* bpb);
void copy_out_file(FILE *fd, uint16_t cluster, uint32_t bytes_remaining,
		   uint8_t *image_buf, struct bpb33* bpb);
void copyout(char *infilename, char* outfilename,
	     uint8_t *image_buf, struct bpb33* bpb);
uint16_t copy_in_file(FILE* fd, uint8_t *image_buf, struct bpb33* bpb, 
		      uint32_t *size);
void write_dirent(struct direntry *dirent, char *filename, 
		   uint16_t start_cluster, uint32_t size);
void copyin(char *infilename, char* outfilename,
	     uint8_t *image_buf, struct bpb33* bpb);