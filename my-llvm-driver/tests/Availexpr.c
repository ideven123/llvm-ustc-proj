int a=1, b=2, c=3, d=4, e=5;

int main(){
    c = a + b;
    d = a + b;
    e = -a;
    b = c - d;
    a = b + e;
    c = b + e;
    for (int i = 0; i < 5; i++){
        if (b<a) b = b*2;
        else b = a+b;
    }
    return 0;
}