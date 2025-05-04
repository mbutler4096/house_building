import subprocess
import time
import numpy as np
import sys


#random values
grid = np.random.randint(0, 100, (10, 10))
np.fill_diagonal(grid, 100)

#gradient
# Random starting values per row (between 1 and 50)
#start_values = np.random.randint(1, 50, (1000, 1))
# Random step increases (1 to 5)
#steps = np.random.randint(1, 5, (1000, 1000))
# Cumulative sum with row-wise increase
#grid = np.cumsum(steps, axis=1) + start_values
# Cap the values at 100
#grid = np.minimum(grid, 100)

print(grid)
np.savetxt('large_max_brute_force.txt', grid, fmt='%d')
