#include <iostream>

using namespace std;

int main()
{
    long long n1,n2;
    long long m;
    cin >> n1 >> n2;
    int arr[n1];

    for (int i = 0; i < n1; i++){
        std::cin >> arr[i];
    }

    for (int i = 1; i < n1; i++){
        if ((2*n2 +2) < (arr[i] - arr[i-1]+ n2+1)){
            m += (2*n2 + 2);
        }

        else{
            m += (n2 + 1 + arr[i] - arr[i-1]);
        }
    }
    cout << m;

}
