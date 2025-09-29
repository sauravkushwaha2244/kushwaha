#include<iostream>
using namespace std;
int main(){
    int a,b,c;
    cin>>a>>b>>c; //variable declration and initialization
    
    if (a>=b && a>=c)
        cout<< "\n a is greatest";
    else if (b>=a && b>=c)
        cout<< "\n b is greatest";
    else
        cout<< "\n c is greatest";
    return 0;
    
}