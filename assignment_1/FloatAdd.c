#include "FloatAdd.h"

int FloatAdd(float a_in, float b_in)
{
    myFloat a, b, ans;
    int i;
    int kp = 0;
    int ans_out_mask;
    int* a_ptr = &a_in;    //如果不转换成int型，有的编译器会在移位的时候智能地改成除2
    int* b_ptr = &b_in;

    ans.s = 0;

    a.m = (*a_ptr & 0x007fffff) | 0x00800000;
    if (*a_ptr & 0x80000000)
    {
        a.s = 1;
        a.m = -a.m;
    }

    b.m = (*b_ptr & 0x007fffff) | 0x00800000;
    if (*b_ptr & 0x80000000)
    {
        b.s = 1;
        b.m = -b.m;
    }

    a.n = ((*a_ptr >> 23) & 0x000000ff) - 127;
    b.n = ((*b_ptr >> 23) & 0x000000ff) - 127;

    Allign(&a, &b, &kp, &ans);

    ans.m = a.m + b.m;
    if (ans.m < 0)
    {
        ans.s = 1;
        ans.m = -ans.m;
    }

    if (ans.m || ans.n)
    {
        if (ans.m & 0x01000000)
        {
            ans.m >>= 1;
            ans.n++;
            ans.m &= 0xff7fffff;
        }
        else
        {
            for (i = 0; i < 24; ++i)
            {
                if (ans.m & 0x00800000)
                {
                    ans.m &= 0xff7fffff;
                    break;
                }
                else
                {
                    ans.m <<= 1;
                    ans.m += kp & 0x00000001;
                    kp >>= 1;
                    ans.n--;
                }
                
            }
        }        
    }

    ans_out_mask = ans.m + ((ans.n + 127) << 23) + (ans.s << 31);
    return ans_out_mask;

}

void Allign(myFloat* a, myFloat* b, int* kp, myFloat* ans)
{
    int diff = 0;
    int i;

    if (a->n > b->n)
    {
        diff = a->n - b->n;
        ans->n = a->n;
        for (i = 0; i < diff && b->m; ++i)
        {
            *kp = (*kp << 1) + (b->m & 0x00000001);
            b->m >>= 1;
        }
    }
    else
    {
        diff = b->n - a->n;
        ans->n = b->n;
        for (i = 0; i < diff && a->m; ++i)
        {
            *kp = (*kp << 1) + (a->m & 0x00000001);
            a->m >>= 1;
        }
    }

    return;
    
}