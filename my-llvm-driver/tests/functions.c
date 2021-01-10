#include <stdio.h>
int getint(){
    int n;
    scanf("%d", &n);
    return n;
}
int getfloat(){
    printf("char\n");
    return 0;
}
int main(){
    int x = getint();
    int y = getfloat();
    return 0;
}