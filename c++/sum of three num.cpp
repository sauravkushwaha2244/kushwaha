#include<iostream>
using namespace std;
int main(){
    int num,sum=0,digit;
cout<<"enter the the digit number number";
cin>>num;
while(num>0){
    digit = num % 10;
    sum += digit;
    num /= 10;
}
return 0;
}