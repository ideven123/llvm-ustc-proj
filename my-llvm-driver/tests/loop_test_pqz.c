int main(){
    int j = 5;
    L1:
    while(j > 0){
        for(int i=0;i<10;i++){
            i++;
            for(int i=0;i<10;i++){
                i++;
            }
        }
        j--;
    }
    for(int i=0;i<10;i++){
        i++;
    }
    goto L1;
    return 0;
}