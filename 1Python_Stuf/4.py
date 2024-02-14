n, k = map(int, input().split())
d = list(map(int, input().split()))

cost = 0
prev_end = 0
for i in range(n):
    if d[i] > prev_end:
        days = d[i] - prev_end
        subs = (days + k - 1) // k
        cost += subs * (k + days)
        prev_end += subs * k

print(cost - k)
