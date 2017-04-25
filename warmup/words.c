#include "common.h"

int main(int argc, char* argv[])
{
    int i;
    /* NOTE:
        The first argument passed to the Main function when running the program 
        is the name of the program that was called
     */
    for(i=1;i<argc;i++)
    {
        printf("%s\n",argv[i]);
    }
    return 0;
}
