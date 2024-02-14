import sys

#int1 = map(int,(sys.stdin.readline()))
nums = list(map(int, sys.stdin.readline().split()))
nums1 = []
nums2 = [0 for i in range(nums[1])]
for i in range(0,nums[0]):
    nums1 = list(map(int, sys.stdin.readline().split()))
    nums +=nums1
    nums1 = []
for i in range (1, nums[1]):
    nums1.append(i)
print (nums1)


"""count = 0
nums1,nums2 = [],[]

for i in nums:
    for j in nums:
        if j < i:
            continue
        else:
            count += i
    nums1.append(count)
    nums2.append(i)
    count = 0

y = max(nums1)
z = nums2[nums1.index(y)] 
print("Total Dollars:", y,"Each person:", z)"""
