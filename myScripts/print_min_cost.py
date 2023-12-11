import csv
import heapq
import sys

def find_min_cost_row(file_path):
    with open(file_path, 'r') as csvfile:
        reader = csv.DictReader(csvfile)
        rows = list(reader)

        if not rows:
            print("No data in the file.")
            return

        # min_cost_row = min(rows, key=lambda x: float(x['cost']))
        min_cost_rows = heapq.nsmallest(5, rows, key=lambda x: float(x['cost']))

        print("Rows with Minimum Cost:")
        print("cache_eviction_policy, cache_size, gib_missed, cost")
        # print(f"{min_cost_row['cache_eviction_policy']}, {min_cost_row['cache_size']}, {min_cost_row['gib_missed']}, {min_cost_row['cost']}")
        for row in min_cost_rows:
            print(f"{row['cache_eviction_policy']}, {row['cache_size']}, {row['gib_missed']}, {row['cost']}")


# Load CSV data
if len(sys.argv) < 2:
    print("Usage: python3 print_min_cost.py <3_day_data>")
    sys.exit(1)

file_path = sys.argv[1]

find_min_cost_row(file_path)
