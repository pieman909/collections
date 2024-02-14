#include <bits/stdc++.h>
using namespace std;

int main()
{
    long N, T;
    unordered_map<int, int> mp; //day -> no. of haybales delivered
    cin >> N >> T;

    for(int i = 0; i < N; i++) {
        long d, b;
        cin >> d >> b;
        mp[d] = b;
    }

    int haybales = 0;
    int day = 1;
    int res = 0;

    while(day <= T) {
        haybales += mp[day];

        if(haybales > 0) {
            res++;
            haybales--;
        }

        day++;
    }

    cout << res;

    return 0;
}
