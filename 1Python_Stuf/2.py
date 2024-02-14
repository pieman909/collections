import sys
nums = list(map(int, sys.stdin.readline().split()))
nums1 = []
nums2 = [0 for i in range(0,nums[1])]

for i in range(0,nums[0]):
    nums1 = list(map(int, sys.stdin.readline().split()))
    nums +=nums1
    nums1 = []
for i in range (0, nums[1]):

    nums1.insert(i,1)

return sum(nums2)
