int main(){
    int num = 0 ,i,j,k;
    for(int i=0;i<10;i++){
        for(int j=i;j<10;j++){
            num++;
        }
    }
    for(int j=i;j<10;j++){
            for(int k=j;k<10;k++){
				num++;
            }
        }
    for(int k=j;k<10;k++){
				num++;
     } 
        for(int i=0;i<10;i++){
        for(int j=i;j<10;j++){
            for(int k=j;k<10;k++){
				num++;
            }
        }
        for(int j=i;j<10;j++){
            for(int k=j;k<10;k++){
				num++;
            }
        }
    }   
    return num;
}