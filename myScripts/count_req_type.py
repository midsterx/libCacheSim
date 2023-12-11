import csv
import sys

# Load CSV data
if len(sys.argv) < 2:
    print("Usage: python3 count_req_type.py <trace>")
    sys.exit(1)

file_path = sys.argv[1]

op_type_counts = {'Put': 0, 'Get': 0, 'Delete': 0, 'Head': 0, 'Copy': 0}

with open(file_path, 'r') as file:
    reader = csv.reader(file)

    # Skip the header line
    next(reader, None)

    prev_row = None

    for row in reader:
        op_type = int(row[1])
        if op_type == 0:
            op_type_counts['Put'] += 1
        elif op_type == 1:
            op_type_counts['Get'] += 1
        elif op_type == 2:
            print(row)
            if prev_row:
                print(prev_row)
            op_type_counts['Delete'] += 1
            break
        elif op_type == 3:
            op_type_counts['Head'] += 1
        elif op_type == 4:
            op_type_counts['Copy'] += 1
        prev_row = row

print("Operation Type Counts:")
for op_type, count in op_type_counts.items():
    print(f"{op_type}: {count}")
