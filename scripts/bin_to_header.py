#!/usr/bin/env python
"""
bin_to_header.py — Convert a binary file to a C byte-array header.

Usage:
    python bin_to_header.py <input.spv> <array_name> <output.h>

Example:
    python bin_to_header.py triangle.vert.spv triangle_vert_spirv triangle_vert_spirv.h

Produces a header like:
    static const unsigned char triangle_vert_spirv[] = { 0x03, 0x02, ... };
    static const unsigned int triangle_vert_spirv_size = sizeof(triangle_vert_spirv);
"""

import sys
import os

def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <input> <array_name> <output.h>")
        sys.exit(1)

    input_path, array_name, output_path = sys.argv[1], sys.argv[2], sys.argv[3]

    with open(input_path, "rb") as f:
        data = f.read()

    basename = os.path.basename(input_path)

    with open(output_path, "w") as f:
        f.write(f"/* Auto-generated from {basename} -- do not edit by hand. */\n")
        f.write(f"static const unsigned char {array_name}[] = {{\n")

        for i in range(0, len(data), 12):
            chunk = data[i:i+12]
            hex_values = ", ".join(f"0x{b:02x}" for b in chunk)
            f.write(f"    {hex_values},\n")

        f.write("};\n")
        f.write(f"static const unsigned int {array_name}_size = sizeof({array_name});\n")

    print(f"Wrote {len(data)} bytes as {array_name} to {output_path}")

if __name__ == "__main__":
    main()
