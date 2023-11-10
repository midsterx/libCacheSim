import argparse
import sys

import pandas as pd

# Check if enough command line arguments are provided
# file2 should have greater number of bytes missed than file 1
# For example, file 1 can be 4 day trace data and file 2 can be 7 day trace data
if len(sys.argv) < 3:
    print('Usage: python3 script.py <4_day_data> <7_day_data> [--filter]')
    sys.exit(1)

# Get file paths from command line arguments
parser = argparse.ArgumentParser()
parser.add_argument('file1_path', type=str, help='4-day data path')
parser.add_argument('file2_path', type=str, help='7-day data path')
parser.add_argument('--filter_mac8', action='store_true')
args = parser.parse_args()

file1_path = args.file1_path
file2_path = args.file2_path

# Read data from files into pandas DataFrames
df1 = pd.read_csv(file1_path, header=None, names=['cache_eviction_policy', 'cache_size', 'number_of_requests', 'miss_ratio', 'gib_missed', 'cost'])
df2 = pd.read_csv(file2_path, header=None, names=['cache_eviction_policy', 'cache_size', 'number_of_requests', 'miss_ratio', 'gib_missed', 'cost'])

# Merge the two DataFrames on cache_eviction_policy and cache_size
merged_df = pd.merge(df1, df2, on=['cache_eviction_policy', 'cache_size'])

if (args.filter_mac8):
    mac8_policies = ['ARC', 'Clock', 'Hyperbolic', 'FIFO', 'LFU', 'LHD', 'LRU', 'TwoQ']
    merged_df = merged_df[merged_df['cache_eviction_policy'].isin(mac8_policies)]

# Compute the difference in bytes missed in GiB
merged_df['gib_missed'] = round(merged_df['gib_missed_y'] - merged_df['gib_missed_x'], 4)

# Add the 'cost' column
egress_costs = 0.09  # $/GB
storage_costs = 0.023  # $/GB month
merged_df['cost'] = (merged_df['gib_missed'] * egress_costs) + (merged_df['cache_size'] * storage_costs)

# Select the columns for the result file
result_df = merged_df[['cache_eviction_policy', 'cache_size', 'gib_missed', 'cost']]

output_file = file2_path
output_file += '_3_cost'

if (args.filter_mac8):
    output_file += '_mac8_filtered'

result_df.to_csv(output_file, index=False)
