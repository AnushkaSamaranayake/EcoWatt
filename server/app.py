from flask import Flask, request, jsonify
import os, base64, json, time

UPLOAD_DIR = "uploads"
os.makedirs(UPLOAD_DIR, exist_ok=True)

app = Flask(__name__)

@app.route("/upload", methods=["POST"])
def upload():
    if not request.is_json:
        return jsonify({"status":"error","reason":"expected json"}), 400
    data = request.get_json()
    device = data.get("device_id", "unknown")
    sample_count = data.get("sample_count", 0)
    payload_format = data.get("payload_format", "")
    payload = data.get("payload","")
    ts = time.time()
    # Save raw payload
    fname = f"{UPLOAD_DIR}/{device}_{int(ts)}.json"
    with open(fname, "w") as f:
        json.dump({"device":device,"ts":ts,"count":sample_count,"format":payload_format,"payload":payload}, f)
    # Return ack
    return jsonify({"status":"ok","ack_ts":int(ts)})

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
