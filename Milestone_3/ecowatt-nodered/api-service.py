from flask import Flask, request, jsonify
import requests
import os
from dotenv import load_dotenv

#Load environment variables from .env file
load_dotenv()

app = Flask(__name__)

NODERED_URL_SEND_DATA = "http://127.0.0.1:1880/api/v1/upload"
NODERED_URL_GET_DATA = "http://127.0.0.1:1880/api/v1/history/:device_id"

API_KEYS = {
    "EcoWatt001" : os.getenv("API_KEY_1"),
    "EcoWatt002" : os.getenv("API_KEY_2"),
    "EcoWatt003" : os.getenv("API_KEY_3"),
    "EcoWatt004" : os.getenv("API_KEY_4"),
    "EcoWatt005" : os.getenv("API_KEY_5"),
    }
    

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
    if not data:
        return jsonify({"error": "No data provided"}), 400
    
    try:
        response = requests.post(NODERED_URL_SEND_DATA, json=data)
        response.raise_for_status()
        return jsonify({
            "message": "Data uploaded successfully",
            "node_red_response": response.text
        }), response.status_code
    
    except requests.exceptions.RequestException as e:
        return jsonify({"error": str(e)}), 500



if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)