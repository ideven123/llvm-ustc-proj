int main()
{
    int x = 1;
    if(x < 0){
        x = 2;
    }

    int val1 = x + 0;
    int val2 = x * 1;
    int val3 = 2 * x;
    int val4 = x / 2;

    int val5 = x + 1;
    int val6 = val5 - 1;
    int val7 = val6 + 8;

    int temp = val1 + val2 + val3 + val4 + val5 + val6 + val7;
    return temp;
}