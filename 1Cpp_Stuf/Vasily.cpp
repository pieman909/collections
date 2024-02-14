#include <iostream>

int main(){
    int a, b, c;
    std::cin >> a >> b;
    
    while (isdigit(a)){
        c = c+a;
        a = a/b;
    }
    return c;
}