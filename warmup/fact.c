#include "common.h"

int fact_helper(int value);

int main(int argc,char* argv[])
{
    //printf("The first argument passed to the function is %s \n",argv[0]);
    int value = 0;
    int i;
    int is_error = 0;
    int is_stop = 0;
    //int len = strlen(argv[1]);
    for(i=0;(argv[1])[i] != '\0';i++)
    {
        if((argv[1])[i]<'0'||(argv[1])[i]>'9')
        {
            is_error = 1;
            break;
        }
        if(value > 12)
        {
            is_stop = 1;
            break;
        }
        value = value*10 + ((argv[1])[i] - '0');
    }
    if(is_error == 1)
        printf("Huh?\n");
    else if(is_stop == 1)
        printf("Overflow\n");
    else
        printf("%d\n",fact_helper(value));
    return 0;
}
int fact_helper(int value)
{
    if(value == 1)
        return 1;
    else
        return value*fact_helper(value-1);
}
