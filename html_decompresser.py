
import gzip
import re
import os

# Change this if your array lives in a file other than camera_index.h
source_file = "camera_index.h" 

if not os.path.exists(source_file):
    print(f"Error: Could not find '{source_file}' in this directory.")
    exit(1)

print(f"Reading {source_file}...")
with open(source_file, "r", encoding="utf-8") as f:
    content = f.read()

# Regex to grab everything inside the index_ov5640_html_gz array brackets
match = re.search(r'index_ov5640_html_gz\[\]\s*=\s*\{([^}]+)\}', content)

if match:
    hex_raw = match.group(1)
    # Pull out every 0xXX hex value
    hex_values = re.findall(r'0x[0-9a-fA-F]{2}', hex_raw)
    
    # Convert hex strings to actual bytes
    byte_data = bytes(int(h, 16) for h in hex_values)
    
    print(f"Found {len(byte_data)} bytes. Decompressing...")
    try:
        decompressed = gzip.decompress(byte_data)
        output_name = "index_ov5640.html"
        
        with open(output_name, "wb") as out:
            out.write(decompressed)
            
        print(f"🎉 Success! Extracted original file to: {output_name}")
    except Exception as e:
        print(f"Decompression failed: {e}")
else:
    print("Could not find the 'index_ov5640_html_gz' array inside the file.")