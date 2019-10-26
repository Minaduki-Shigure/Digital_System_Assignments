#include <stdio.h>
#include <string.h>

typedef struct myFloat
{
    int n;
    int m;
    int s;
} myFloat;

//void FloatAdd(float a_in, float b_in, float* ans_out);
int FloatAdd(float a_in, float b_in);
void Allign(myFloat* a, myFloat* b, int* kp, myFloat* ans);