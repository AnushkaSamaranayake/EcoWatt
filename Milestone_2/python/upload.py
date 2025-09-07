import json
import zlib


def compress_data(data):
    """Compress the data using zlib after converting to JSON bytes."""
    json_bytes = json.dumps(data).encode('utf-8')
    compressed = zlib.compress(json_bytes)
    return compressed

def upload(data):
    """Simulate uploading data to cloud."""
    compressed = compress_data(data)
    print("\n--- Uploading to Cloud ---")
    print("Data:", json.dumps(data, indent=2))  # Pretty print for readability
    print("Upload successful — Cloud ACK received.")
    print("---------------------------\n")