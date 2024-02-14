def stdev (data):
    n = len(data)
    mean = sum(data)/n
    deviations = [(x - mean) ** 2 for x in data]
    variance = (sum(deviations) / n)**0.5
    return variance

if __name__ == '__main__':
    data = [213,13123,41,32,213,213,1,33,13,1,2,124,1,41,231,321314,4124512,12412,312,312,31,12,341,24,1,21,23231,124, 1,24]
    stdev1 = stdev (data)
    print ( "The standard deviation of the data is:", stdev1)
