

import math

#* 
def G(x,y,stdev):
    coors = x*x + y*y
    stdevSq = stdev*stdev
    frac = (-1.0 * coors)/(stdevSq*2.0)
    g = (1.0 / (2.0 * math.pi * stdevSq)) * math.exp(frac)
    return g

kernelSize = 11
halfSize = kernelSize // 2
stdev = 10

result = ""


kernel = [0] * (kernelSize * kernelSize)
sum = 0.0

#* get values
for i in range(kernelSize):
    for j in range(kernelSize):
        g = G(i - halfSize, j - halfSize, stdev)
        kernel[i * kernelSize + j] = g
        sum += g

#* normalize
for i in range(kernelSize):
    for j in range(kernelSize):
        kernel[i * kernelSize + j] /= sum
        kernel[i * kernelSize + j] = round(kernel[i * kernelSize + j], 4)

print(kernel)