#include "Fat12.h"

void usage(void);
void print_indent_n(int indent);
void follow_dir(uint16_t cluster, int indent, uint8_t *image_buf, struct bpb33* bpb);