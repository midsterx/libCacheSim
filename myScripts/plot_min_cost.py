import math
import sys

import pandas as pd
import matplotlib.pyplot as plt

# Load CSV data
if len(sys.argv) < 2:
    print("Usage: python3 script.py <3_day_data>")
    sys.exit(1)

file_path = sys.argv[1]

df = pd.read_csv(file_path)

# Group the data by Cache Eviction Policy
grouped = df.groupby('cache_eviction_policy')

# Create a line graph for each Policy with shaded valleys

for name, group in grouped:
    x = group['cache_eviction_policy']
    y = group['cost'].min()

    # Create the line graph
    plt.bar(x, y)

    # plt.text(x.iloc[0], round(y, 2), str(round(y, 2)), ha='center', va='bottom')

    # Rotate x-axis labels for better readability
    plt.xticks(rotation=90)  # Rotate the labels by 90 degrees

# Set the y-axis limits to start from 0
plt.ylim(0, math.ceil(grouped['cost'].max().max() / 200) * 200)



plt.xlabel('Cache Eviction Policy')
plt.ylabel('Cost ($)')
plt.title('Cache Eviction Policy vs. Minimum Cost')
plt.grid(True)

plt.savefig(file_path + '_min.png', bbox_inches='tight')
