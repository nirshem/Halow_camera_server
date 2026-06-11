import gzip
import re
import os

html_file = "index_ov5640.html"
source_file = "camera_index.h"

# Safety checks
if not os.path.exists(html_file):
    print(f"Error: Could not find '{html_file}' to compress.")
    exit(1)
if not os.path.exists(source_file):
    print(f"Error: Target file '{source_file}' does not exist.")
    exit(1)

print(f"Reading {html_file}...")
with open(html_file, "rb") as f:
    html_data = f.read()

print("Compressing HTML payload with gzip...")
compressed_data = gzip.compress(html_data)
new_len = len(compressed_data)

print("Formatting bytes into C-array syntax...")
hex_bytes = [f"0x{b:02X}" for b in compressed_data]
lines = []
for i in range(0, len(hex_bytes), 16):
    chunk = ", ".join(hex_bytes[i:i+16])
    if i + 16 < len(hex_bytes):
        lines.append("  " + chunk + ",")
    else:
        lines.append("  " + chunk)
hex_body = "\n".join(lines)

print(f"Parsing {source_file}...")
with open(source_file, "r", encoding="utf-8") as f:
    content = f.read()

# 1. Update the #define length macro safely using \g<1>
content, count_len = re.subn(
    r'(#define\s+index_ov5640_html_gz_len\s+)\d+',
    rf'\g<1>{new_len}',
    content
)

# 2. Update the actual uint8_t array structure safely using \g<1> and \g<2>
content, count_arr = re.subn(
    r'(const\s+uint8_t\s+index_ov5640_html_gz\[\]\s*=\s*\{)[^}]+(\};)',
    rf'\g<1>\n{hex_body}\n\g<2>',
    content
)

# Write back changes only if both elements were safely located
if count_len > 0 and count_arr > 0:
    with open(source_file, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"🎉 Success! Updated '{source_file}' with new payload ({new_len} compressed bytes).")
else:
    print("❌ Error: Failed to auto-update target strings inside header file.")
    if count_len == 0: 
        print("   -> Target missing: '#define index_ov5640_html_gz_len'")
    if count_arr == 0: 
        print("   -> Target missing: 'const uint8_t index_ov5640_html_gz[]'")