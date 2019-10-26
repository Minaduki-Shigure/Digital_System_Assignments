#include "FloatAdd.h"

int main(int argc, char* argv[])
{
    float a, b;
    int c;
    float* ans;
    scanf("%f %f", &a, &b);
    c = FloatAdd(a, b);
    ans = (float*) &c;
    printf("%f\n", *ans);
    return 0;
}