# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import argparse
import csv
import json


def read_csv_to_c_and_header(file_path, scaling_params_path, output_c_file, output_h_file):
    copyright_header = """/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
"""

    # Read CSV data
    with open(file_path, mode='r', newline='') as csvfile:
        reader = csv.reader(csvfile)
        next(reader)  # Skip header row

        data = []
        for row in reader:
            float_row = [float(item) for item in row]
            data.append(float_row)

        num_rows = len(data)
        num_cols = len(data[0])

    # Load scaling parameters from JSON
    with open(scaling_params_path, 'r') as f:
        scaling_params = json.load(f)
        min_vals = scaling_params['min']
        max_vals = scaling_params['max']

    # Generate the .c file content
    c_array_str = copyright_header
    c_array_str += f'#include "test_data.h"\n\n'
    c_array_str += f'const float test_data[{num_rows}][{num_cols}] = {{\n'
    for row in data:
        c_array_str += '    { '
        c_array_str += ', '.join([f'{item:.5f}' for item in row])
        c_array_str += ' },\n'
    c_array_str += '};\n\n'

    # Add scaling parameters to .c file
    c_array_str += f'const float scaling_min[{len(min_vals)}] = {{ '
    c_array_str += ', '.join([f'{item:.5f}' for item in min_vals])
    c_array_str += ' };\n\n'

    c_array_str += f'const float scaling_max[{len(max_vals)}] = {{ '
    c_array_str += ', '.join([f'{item:.5f}' for item in max_vals])
    c_array_str += ' };\n'

    # Write to the .c file
    with open(output_c_file, mode='w') as cfile:
        cfile.write(c_array_str)

    # Generate the .h file content
    h_array_str = copyright_header
    h_array_str += f'#ifndef TEST_DATA_H\n'
    h_array_str += f'#define TEST_DATA_H\n\n'
    h_array_str += f'extern const float test_data[{num_rows}][{num_cols}];\n\n'
    h_array_str += f'extern const float scaling_min[{len(min_vals)}];\n\n'
    h_array_str += f'extern const float scaling_max[{len(max_vals)}];\n\n'
    h_array_str += f'#endif // TEST_DATA_H\n'

    # Write to the .h file
    with open(output_h_file, mode='w') as hfile:
        hfile.write(h_array_str)


def main():
    parser = argparse.ArgumentParser(description='Process CSV files and generate C and header arrays.')
    parser.add_argument('csv_files', nargs='*', help='Paths to the CSV files')
    parser.add_argument('--output_c', help='Output C file name', default=None)
    parser.add_argument('--scaling_params', required=True, help='Path to the scaling parameters JSON file')

    args = parser.parse_args()

    for csv_file in args.csv_files:
        if args.output_c:
            output_c_file = args.output_c
            output_h_file = args.output_c.replace('.c', '.h')
        else:
            output_c_file = csv_file.replace('.csv', '.c')
            output_h_file = csv_file.replace('.csv', '.h')

        print(f'Generating C file: {output_c_file}')
        print(f'Generating Header file: {output_h_file}')
        read_csv_to_c_and_header(csv_file, args.scaling_params, output_c_file, output_h_file)


if __name__ == '__main__':
    main()
