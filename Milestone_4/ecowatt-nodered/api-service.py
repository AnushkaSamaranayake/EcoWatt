from flask import Flask, json, request, jsonify
import requests
import os, hashlib, hmac, secrets, math, base64
from dotenv import load_dotenv
from typing import Tuple

#Load environment variables from .env file
load_dotenv()

app = Flask(__name__)

NODERED_URL_SEND_DATA = "http://10.104.204.251:1880/api/v1/upload"
NODERED_URL_GET_DATA = "http://10.104.204.251:1880/api/v1/history/:device_id"

API_KEYS = {
    "EcoWatt001" : os.getenv("API_KEY_1"),
    "EcoWatt002" : os.getenv("API_KEY_2"),
    "EcoWatt003" : os.getenv("API_KEY_3"),
    "EcoWatt004" : os.getenv("API_KEY_4"),
    "EcoWatt005" : os.getenv("API_KEY_5"),
    }

    
@app.route("/")
def index():
    return "EcoWatt API Service is running"

@app.route("/api/ecowatt/cloud/upload", methods=["POST"])
def uploadData():

    #get headers
    device_id = request.headers.get("x-device-id")
    token = request.headers.get("x-api-key")

    #validate headers
    if not device_id or not token:
        print(f"Received upload request from device_id: {device_id}")
        return jsonify({"error": "Missing device_id or token in headers"}), 400
    
    if API_KEYS.get(device_id) != token:
        print(f"Received upload request from device_id: {device_id}")
        print(f"Unauthorized access attempt by token: {token}")
        return jsonify({"error": "Unauthorized"}), 403

    #get json data
    data = request.json
    
    #print json dump with indent 4
    print(json.dumps(data, indent=4))

    #extract relevant fields for Node-Red

    device_id = data.get("device_id")
    interval_start = data.get("interval_start")
    voltage = data.get("voltage")
    current = data.get("current")

    print(type(voltage))
    print(type(current))

    # for i in range(len(voltage)):
    #     voltage += voltage[i]
    
    # voltage_avg = voltage / len(voltage)

    # for i in range(len(current)):
    #     current += current[i]

    # current_avg = current / len(current)

    data_node_red = {
        "device_id":device_id,
        "timestamp":interval_start,
        "voltage":voltage,
        "current":current
    }

    print("Data to be sent to Node-Red:")
    print(json.dumps(data_node_red, indent=4))

    if not data_node_red:
        return jsonify({"error": "No data provided"}), 400
    
    try:
        print("sending data to Node-Red...")
        response = requests.post(NODERED_URL_SEND_DATA, json=data_node_red)
        response.raise_for_status()
        return jsonify({
            "message": "Data uploaded successfully",
            "node_red_response": response.text
        }), response.status_code
    
    except requests.exceptions.RequestException as e:
        print(f"Error sending data to Node-Red: {e}")
        return jsonify({"error": str(e)}), 500

#=============== FOTA update endpoint ===============

# Configuration
UPLOAD_DIR = "./firmware"
ACTIVE_VERSION = None
ACTIVE_PATH = None
CHUNK_SIZE = 1024  # bytes

ENC_KEY = bytes([0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe, 0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81])
HMAC_KEY = bytes([0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe, 0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7, 0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4])

# Per-active version metadata
ACTIVE_IV = None     # 16 bytes
ACTIVE_SIZE = None
ACTIVE_SHA256 = None
TOTAL_CHUNKS = None

def ensure_active_ready():
    if not (ACTIVE_VERSION and ACTIVE_PATH and ACTIVE_IV and ACTIVE_SHA256):
        raise RuntimeError("Active firmware is not set")

def sha256_file(path:str)->str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for blk in iter(lambda: f.read(8192), b""):
            h.update(blk)
    return h.hexdigest()

def load_active_meta():
    global ACTIVE_SIZE, ACTIVE_SHA256, TOTAL_CHUNKS
    ACTIVE_SIZE = os.path.getsize(ACTIVE_PATH)
    ACTIVE_SHA256 = sha256_file(ACTIVE_PATH)
    TOTAL_CHUNKS = math.ceil(ACTIVE_SIZE / CHUNK_SIZE)

def hexlify(b:bytes)->str: return b.hex()

def aes_ctr(data:bytes, key:bytes, iv:bytes, block_offset:int)->bytes:
    """
    Simple CTR using Python's Cryptodome if available; if not, manual AES-ECB keystream.
    To keep this self-contained, we implement with PyCryptodome if present; else fallback to a naive xor with hashlib (DEV ONLY).
    """
    try:
        from Crypto.Cipher import AES
        # advance counter by block_offset in last 32 bits
        ctr = bytearray(iv)
        ctr_tail = int.from_bytes(ctr[12:16], "big")
        ctr_tail = (ctr_tail + block_offset) & 0xFFFFFFFF
        ctr[12:16] = ctr_tail.to_bytes(4, "big")

        out = bytearray(len(data))
        off = 0
        aes = AES.new(key, AES.MODE_ECB)
        while off < len(data):
            ks = bytearray(aes.encrypt(bytes(ctr)))
            chunk = min(16, len(data) - off)
            for i in range(chunk):
                out[off+i] = data[off+i] ^ ks[i]
            # inc counter
            ctr_tail = (int.from_bytes(ctr[12:16], "big") + 1) & 0xFFFFFFFF
            ctr[12:16] = ctr_tail.to_bytes(4, "big")
            off += chunk
        return bytes(out)
    except Exception:
        # DEV fallback (not secure): xor with H( iv || block_index )
        out = bytearray(data)
        off = 0
        block_index = block_offset
        while off < len(data):
            ks = hashlib.sha256(iv + block_index.to_bytes(4, "big")).digest()
            chunk = min(16, len(data) - off)
            for i in range(chunk):
                out[off+i] ^= ks[i]
            off += chunk
            block_index += 1
        return bytes(out)

@app.route("/upload_firmware", methods=["POST"])
def upload_firmware():
    global ACTIVE_VERSION, ACTIVE_PATH, ACTIVE_IV
    f = request.files.get("file")
    version = request.form.get("version")
    if not f or not version:
        return jsonify({"error": "missing file or version"}), 400

    os.makedirs(UPLOAD_DIR, exist_ok=True)
    save_path = os.path.join(UPLOAD_DIR, f"firmware_v{version}.bin")
    f.save(save_path)

    # Rotate new IV for this version
    ACTIVE_VERSION = version
    ACTIVE_PATH = save_path
    ACTIVE_IV = secrets.token_bytes(16)

    load_active_meta()

    return jsonify({
        "status": "success",
        "version": ACTIVE_VERSION,
        "size": ACTIVE_SIZE,
        "hash": ACTIVE_SHA256,         # <- reference key name
        "chunk_size": CHUNK_SIZE,
        "iv": ACTIVE_IV.hex(),
        "total_chunks": TOTAL_CHUNKS
    })

@app.route("/manifest", methods=["GET"])
def manifest():
    ensure_active_ready()
    return jsonify({
        "version": ACTIVE_VERSION,
        "size": ACTIVE_SIZE,
        "hash": ACTIVE_SHA256,       # reference field name
        "chunk_size": CHUNK_SIZE,
        "iv": ACTIVE_IV.hex(),
        "total_chunks": TOTAL_CHUNKS
    })

def read_chunk(n:int)->bytes:
    with open(ACTIVE_PATH, "rb") as fw:
        fw.seek(n * CHUNK_SIZE)
        return fw.read(CHUNK_SIZE)

@app.route("/chunk", methods=["GET"])
def chunk():
    ensure_active_ready()
    try:
        version = request.args["version"]
        n = int(request.args["n"])
    except Exception:
        return jsonify({"error": "bad request"}), 400

    if version != ACTIVE_VERSION or n < 0 or n >= TOTAL_CHUNKS:
        return jsonify({"error": "no such chunk"}), 404

    plain = read_chunk(n)

    # Encrypt with AES-CTR (offset in blocks = (n*chunk_size)/16)
    block_offset = (n * CHUNK_SIZE) // 16
    cipher = aes_ctr(plain, ENC_KEY, ACTIVE_IV, block_offset)

    # HMAC over n || iv || cipher
    mac = hmac.new(HMAC_KEY, digestmod=hashlib.sha256)
    mac.update(n.to_bytes(4, "big"))
    mac.update(ACTIVE_IV)
    mac.update(cipher)
    tag = mac.hexdigest()

    return jsonify({
        "chunk_number": n,
        "iv": ACTIVE_IV.hex(),
        "data": base64.b64encode(cipher).decode(),
        "mac": tag
    })

@app.route("/report", methods=["POST"])
def report():
    print("REPORT:", request.json)
    return jsonify({"ok": True})

@app.route("/boot_ok", methods=["POST"])
def boot_ok():
    print("BOOT_OK:", request.json)
    return jsonify({"ok": True})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)