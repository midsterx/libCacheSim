import os
import sys
import pandas as pd

if len(sys.argv) < 2:
    print("Usage: python extract_trace_4_days.py <filename>")
    sys.exit(1)

# Read data from file into a pandas DataFrame
file_path = sys.argv[1]  # get filename from command line argument

# timestamp (ms), op type [Put:0, Get:1, Delete:2, Head: 3, Copy: 4], object id, object size (byte)
# above line will become -> timestamp (ms), op type, object id, object size (byte)
with open(file_path, 'r') as f:
    header_line = f.readline().strip().replace("op type [Put:0, Get:1, Delete:2, Head: 3, Copy: 4]", "op type").split('[')[0]
    header_line = header_line.replace("# timestamp (ms)", "timestamp").split('[')[0]

print(header_line)
df = pd.read_csv(file_path, skiprows=1, names=header_line.split(','))

# for row in df.itertuples():
#     if row[2] == 2:
#         target_id = row[3]
#         target_timestamp = row[1]

#         for later_row in df[row[0]+1:].itertuples():
#             if later_row[3] == target_id and later_row[1] > target_timestamp:
#                 print(target_id, target_timestamp, later_row[1])

df = df.dropna(subset=[' object size (byte)'])

# Convert timestamp column to numeric
df['timestamp'] = pd.to_numeric(df['timestamp'], errors='coerce')

# Filter rows based on timestamp
filtered_df = df[df['timestamp'] <= 4 * 24 * 3600 * 1000]

# Write output to new file
output_file = os.path.basename(file_path)
output_file += '_4'
filtered_df.to_csv(output_file, index=False)
