import socket
import os
import sys
import json
from huggingface_hub import InferenceClient

# --- Configuration ---
# IMPORTANT: This application is designed for Linux and will not run on Windows.
if os.name == 'nt':
    print("ERROR: Process Sentinel is designed for Linux and is not compatible with Windows.", file=sys.stderr)
    sys.exit(1)

# Get Hugging Face token from environment variable for security
HF_TOKEN = os.getenv("HF_TOKEN")
if not HF_TOKEN:
    print("ERROR: Hugging Face token not found.", file=sys.stderr)
    print("Please set the HF_TOKEN environment variable.", file=sys.stderr)
    sys.exit(1)

# Use a temporary directory for the socket
SOCKET_PATH = "/tmp/process_sentinel.sock"
MODEL = "mistralai/Mistral-7B-Instruct-v0.2"

# --- AI Setup ---
try:
    client = InferenceClient(model=MODEL, token=HF_TOKEN)
except Exception as e:
    print(f"ERROR: Could not initialize InferenceClient: {e}", file=sys.stderr)
    sys.exit(1)


def get_ai_decision(data):
    # The STRICT prompt ensures the AI doesn't talk too much
    prompt = f"""<s>[INST] <<SYS>>
You are a Linux System Admin Bot. You MUST respond ONLY in this format:
ACTION:[ACTION_TYPE] REASON:[Short explanation]

Allowed ACTION_TYPES: KILL, PAUSE, RESTART, RENICE, IGNORE.
<</SYS>>

Analyze this anomaly:
Process: {data['name']} (PID: {data['pid']})
CPU Usage: {data['cpu']}% (Normal Mean: {data['mean']}%, StdDev: {data['stddev']})
Z-Score: {data['z_score']}

What is your command? [/INST]"""

    try:
        response = client.text_generation(
            prompt, 
            max_new_tokens=50, 
            stop_sequences=["\n"], # Stop the AI from rambling
            temperature=0.1        # Keep it consistent and "robotic"
        )
        return response.strip()
    except Exception as e:
        return "ACTION:[IGNORE] REASON:[API Error]"

def start_ai_server():
    if os.path.exists(SOCKET_PATH): os.remove(SOCKET_PATH)
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(SOCKET_PATH)
    server.listen(1)

    print("🤖 AI Agent ready. Waiting for C Daemon...")

    while True:
        conn, _ = server.accept()
        raw_data = conn.recv(1024).decode()
        if raw_data:
            anomaly_json = json.loads(raw_data)
            print(f"📡 Analyzing PID {anomaly_json['pid']}...")
            
            # Get the structured string from LLM
            recommendation = get_ai_decision(anomaly_json)
            print(f"🧠 Result: {recommendation}")
            
            # Send back to C
            conn.sendall(recommendation.encode())
        conn.close()

if __name__ == "__main__":
    start_ai_server()