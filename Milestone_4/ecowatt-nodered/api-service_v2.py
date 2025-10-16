from flask import Flask, json, request, jsonify
from flask_cors import CORS
import requests
import os, hashlib, hmac, secrets, math, base64
from dotenv import load_dotenv
from typing import Tuple
from collections import defaultdict
import time
from datetime import datetime

#Load environment variables from .env file
load_dotenv()

app = Flask(__name__)

# Enable CORS for React frontend
try:
    CORS(app)
except ImportError:
    # If flask-cors is not installed, add basic CORS headers manually
    @app.after_request
    def after_request(response):
        response.headers.add('Access-Control-Allow-Origin', '*')
        response.headers.add('Access-Control-Allow-Headers', 'Content-Type,Authorization')
        response.headers.add('Access-Control-Allow-Methods', 'GET,PUT,POST,DELETE,OPTIONS')
        return response

# Store inverter data locally instead of forwarding to Node-RED
INVERTER_DATA_STORE = defaultdict(list)
MAX_DATA_POINTS = 100  # Keep last 100 data points per device

API_KEYS = {
    "EcoWatt001" : os.getenv("API_KEY_1"),
    "EcoWatt002" : os.getenv("API_KEY_2"),
    "EcoWatt003" : os.getenv("API_KEY_3"),
    "EcoWatt004" : os.getenv("API_KEY_4"),
    "EcoWatt005" : os.getenv("API_KEY_5"),
    }

CONFIG_QUEUE = {}
COMMANDS_QUEUE = defaultdict(list)

def _device_id_from_path(device_id):
    return device_id

def _unwrap_envelope(req_json):
    nonce = req_json.get("nonce")
    payload_str = req_json.get("payload")
    mac_hex = req_json.get("mac")
    if not (nonce and payload_str and mac_hex):
        raise ValueError("missing envelope fields")
    try:
        inner = json.loads(payload_str)
    except Exception:
        raise ValueError("Inner payload not JSON")
    return nonce, inner, payload_str, mac_hex

# ======= Frontend Server ========

@app.route("/")
def index():
    return "EcoWatt API Service is running"

# ======= Frontend API Endpoints ==========

@app.route("/api/inverters", methods=["GET"])
def get_all_inverters():
    """Get latest data for all inverters"""
    result = []
    for device_id, data_points in INVERTER_DATA_STORE.items():
        if data_points:
            # Get the latest data point
            latest = data_points[-1]
            result.append(latest)
    return jsonify(result)

@app.route("/api/inverters/<device_id>", methods=["GET"])
def get_inverter_data(device_id):
    """Get data for a specific inverter"""
    data_points = INVERTER_DATA_STORE.get(device_id, [])
    if not data_points:
        return jsonify({"error": "No data found for device"}), 404
    
    # Return latest data point
    return jsonify(data_points[-1])

@app.route("/api/inverters/<device_id>/history", methods=["GET"])
def get_inverter_history(device_id):
    """Get historical data for a specific inverter"""
    limit = request.args.get('limit', 50, type=int)
    data_points = INVERTER_DATA_STORE.get(device_id, [])
    
    # Return last 'limit' data points
    return jsonify(data_points[-limit:] if data_points else [])

@app.route("/api/test/inject_sample_data", methods=["POST"])
def inject_sample_data():
    """Inject sample data for testing (development only)"""
    import random
    
    devices = ['EcoWatt001', 'EcoWatt002', 'EcoWatt003', 'EcoWatt004', 'EcoWatt005']
    
    for device in devices:
        # Generate random but realistic values
        voltage = round(220 + random.uniform(-10, 10), 2)
        current = round(10 + random.uniform(-5, 5), 2)
        
        data_point = {
            "device_id": device,
            "timestamp": datetime.now().isoformat(),
            "voltage": voltage,
            "current": current,
            "power": round(voltage * current, 2),
            "sample_count": random.randint(50, 100),
            "payload_format": "json",
            "payload": f"test_data_{device}",
            "status": "Online"
        }
        
        INVERTER_DATA_STORE[device].append(data_point)
        if len(INVERTER_DATA_STORE[device]) > MAX_DATA_POINTS:
            INVERTER_DATA_STORE[device] = INVERTER_DATA_STORE[device][-MAX_DATA_POINTS:]
    
    return jsonify({"message": f"Sample data injected for {len(devices)} devices"})

# ======= Cloud upload ==========

@app.route("/api/ecowatt/cloud/upload", methods=["POST"])
def uploadData():
    # 1) Try headers (optional)
    hdr_device_id = request.headers.get("x-device-id")
    hdr_token     = request.headers.get("x-api-key")

    # 2) Parse incoming JSON (envelope or raw)
    req_json = request.get_json(force=True, silent=True)
    if not isinstance(req_json, dict):
        return jsonify({"error": "invalid json"}), 400

    # 3) If it’s an envelope, unwrap & verify HMAC (use API_KEYS as PSK)
    inner = None
    if "mac" in req_json and ("payload" in req_json or "body" in req_json):
        try:
            nonce, inner, payload_str, mac_hex = _unwrap_envelope(req_json)
        except ValueError as e:
            return jsonify({"error": f"bad envelope: {e}"}), 400

        # Determine device_id (prefer header, else inner payload)
        device_id = hdr_device_id or inner.get("device_id")
        if not device_id:
            return jsonify({"error": "missing device_id"}), 400

        psk = API_KEYS.get(device_id)
        if not psk:
            return jsonify({"error": "unknown device"}), 403

        mac_calc = hmac.new(
            psk.encode("utf-8"),
            payload_str.encode("utf-8"),
            hashlib.sha256
        ).hexdigest()
        if not hmac.compare_digest(mac_calc, mac_hex):
            return jsonify({"error": "bad mac"}), 403

        data = inner
    else:
        # Not an envelope: fall back to raw payload (dev/testing)
        data = req_json
        device_id = hdr_device_id or data.get("device_id")

    # 4) Optional header token check (only if provided)
    if hdr_device_id and hdr_token:
        if API_KEYS.get(hdr_device_id) != hdr_token:
            return jsonify({"error": "unauthorized"}), 403

    # 5) Store data locally for frontend consumption
    device_id = (data or {}).get("device_id") or hdr_device_id
    if device_id and data:
        # Normalize timestamp: device sends ms epoch; fall back to ISO now()
        ts = data.get("interval_start")
        if isinstance(ts, (int, float)):
            # ms -> ISO UTC
            timestamp = datetime.utcfromtimestamp(ts / 1000.0).isoformat() + "Z"
        else:
            timestamp = datetime.utcnow().isoformat() + "Z"

        voltages = data.get("voltage", [])
        currents = data.get("current", [])

        # Allow either arrays OR single numbers
        if isinstance(voltages, list) and isinstance(currents, list) and voltages and currents:
            # Average instantaneous power across samples
            power = sum(v * c for v, c in zip(voltages, currents)) / len(voltages)
        else:
            # Try scalar fallback
            try:
                v = float(voltages if isinstance(voltages, (int, float)) else 0.0)
                c = float(currents if isinstance(currents, (int, float)) else 0.0)
                power = v * c
            except Exception:
                power = 0.0

        data_point = {
            "device_id": device_id,
            "timestamp": timestamp,
            "sample_count": data.get("sample_count", 0),
            "payload_format": data.get("payload_format"),
            "payload": data.get("payload"),
            "voltage": voltages,
            "current": currents,
            "power": round(power, 3),
            "status": "Online",
        }

        # Store in memory (keep only last MAX_DATA_POINTS)
        INVERTER_DATA_STORE[device_id].append(data_point)
        if len(INVERTER_DATA_STORE[device_id]) > MAX_DATA_POINTS:
            INVERTER_DATA_STORE[device_id] = INVERTER_DATA_STORE[device_id][-MAX_DATA_POINTS:]

    # 6) Echo a simple envelope ack back (nonce++), so device can save nonce
    ack = {
        "nonce": (req_json.get("nonce", 0) + 1) if isinstance(req_json, dict) else 0,
        "ok": True
    }
    return jsonify(ack), 200

    
# ============ Remote configuration ============

@app.route("/device/<device_id>/config", methods=["GET"])
def get_config(device_id):
    did = _device_id_from_path(device_id)
    cfg = CONFIG_QUEUE.pop(did, None)
    if not cfg:
        return ("", 204)
    return jsonify({"config_update": cfg})

@app.route("/device/<device_id>/config_ack", methods=["POST"])
def post_config_ack(device_id):
    did = _device_id_from_path(device_id)
    print("CONFIG_ACK", did, request.json)
    return jsonify({"ok": True})

@app.route("/device/<device_id>/commands", methods=["GET"])
def get_commands(device_id):
    did = _device_id_from_path(device_id)
    cmds = COMMANDS_QUEUE.get(did, [])
    if not cmds:
        return ("", 204)
    COMMANDS_QUEUE[did] = []
    if len(cmds) == 1:
        return jsonify(cmds[0])          # {"command": {...}}
    return jsonify({"batch": cmds})     # optional for future

@app.route("/device/<device_id>/command_result", methods=["POST"])
def post_command_result(device_id):
    did = _device_id_from_path(device_id)
    print("COMMAND_RESULT", did, request.json)
    return jsonify({"ok": True})

# --- Simple push helpers (curl these or wire to your UI) ---

@app.route("/push_config/<device_id>", methods=["POST"])
def push_config(device_id):
    did = _device_id_from_path(device_id)
    body = request.get_json(force=True)
    # Expect exactly: {"config_update": {"sampling_interval":..., "registers":[...]}}
    if not body or "config_update" not in body:
        return jsonify({"error": "missing config_update"}), 400
    CONFIG_QUEUE[did] = body["config_update"]
    return jsonify({"queued": True})

@app.route("/push_command/<device_id>", methods=["POST"])
def push_command(device_id):
    did = _device_id_from_path(device_id)
    body = request.get_json(force=True)
    # Expect exactly: {"command": {"action":"write_register", "target_register":"status_flag","value":1}}
    if not body or "command" not in body:
        return jsonify({"error": "missing command"}), 400
    COMMANDS_QUEUE[did].append({"command": body["command"]})
    return jsonify({"queued": True}) 

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
    """Return False if firmware not initialized, instead of raising."""
    if not (ACTIVE_VERSION and ACTIVE_PATH and ACTIVE_IV and ACTIVE_SHA256):
        print("[FOTA] Manifest requested but no active firmware set.")
        return False
    return True

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
    if not ensure_active_ready():
        # 204 = No Content (or 200 with a small JSON marker if you prefer)
        return jsonify({"status": "no_active_firmware"}), 204
    return jsonify({
        "version": ACTIVE_VERSION,
        "size": ACTIVE_SIZE,
        "hash": ACTIVE_SHA256,
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
    if not ensure_active_ready():
        return jsonify({"error": "no active firmware"}), 404
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