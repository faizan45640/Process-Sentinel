import socket
import os
import sys
import json
import time
from huggingface_hub import InferenceClient

# --- Configuration ---
if os.name == 'nt':
    print("ERROR: Process Sentinel is designed for Linux.", file=sys.stderr)
    sys.exit(1)

# Get Hugging Face token
HF_TOKEN = os.getenv("HF_TOKEN")
if not HF_TOKEN:
    print("ERROR: HF_TOKEN environment variable not set.", file=sys.stderr)
    sys.exit(1)

SOCKET_PATH = "/tmp/process_sentinel.sock"
# Using the specific Featherless model ID you requested
MODEL = "mistralai/Mistral-7B-Instruct-v0.2:featherless-ai"

try:
    client = InferenceClient(api_key=HF_TOKEN)
except Exception as e:
    print(f"ERROR: Client init failed: {e}", file=sys.stderr)
    sys.exit(1)

import re

def get_ai_decision(data):
    # Load User Rules
    user_rules = ""
    try:
        with open("ai/user_rules.txt", "r") as f:
            user_rules = f.read().strip()
    except FileNotFoundError:
        pass

    rule_prompt = ""
    if user_rules:
        rule_prompt = f"USER DEFINED RULES (PRIORITY):\n{user_rules}\n\nApply these rules STRICTLY if applicable."

    trigger_reason = data.get("trigger", "Unknown")
    
    messages = [
        {
            "role": "system",
            "content": "You are a Linux System Guardian. You MUST choose exactly ONE of these actions: [KILL, PAUSE, RESTART, RENICE, IGNORE].\n\nRULES:\n1. If you are unsure or just observing, you MUST use ACTION:[IGNORE].\n2. NEVER invent new actions like ANALYZE or REPORT.\n3. Format: ACTION:[TYPE] MSG:[Short layman explanation].\n4. MSG must be friendly and under 15 words."
        },
        {
            "role": "user",
            "content": f"{rule_prompt}\nProcess: '{data['name']}' (PID {data['pid']})\nTrigger: {trigger_reason}\nStats: CPU {data['cpu']}% (Avg {data['mean']}%)\nChoose action."
        }
    ]

    # Retry loop for model stability
    max_retries = 3
    for attempt in range(max_retries):
        try:
            completion = client.chat.completions.create(
                model=MODEL,
                messages=messages,
                max_tokens=150,
                temperature=0.1
            )
            
            response_text = completion.choices[0].message.content.strip()
            
            # --- ROBUST POST-PROCESSING ---
            # 1. Force convert any non-standard action to IGNORE
            valid_actions = ["KILL", "PAUSE", "RESTART", "RENICE", "IGNORE"]
            
            # Find whatever word is after ACTION:
            match = re.search(r"ACTION:\s*\[?(\w+)\]?", response_text, re.IGNORECASE)
            if match:
                detected_action = match.group(1).upper()
                if detected_action not in valid_actions:
                    # Replace the hallucinated action with IGNORE
                    response_text = response_text.replace(match.group(1), "IGNORE")
            
            # 2. Final Safety Check: If ACTION: is missing or malformed, default to IGNORE
            if "ACTION:" not in response_text.upper():
                response_text = f"ACTION:[IGNORE] MSG: {response_text}"

            return response_text

        except Exception as e:
            error_str = str(e)
            if "503" in error_str or "loading" in error_str.lower():
                print(f"⚠️ Model is loading... retrying ({attempt+1}/{max_retries})")
                time.sleep(10)
                continue
            return f"ACTION:[IGNORE] REASON:[HF Error: {error_str[:60]}]"

    return "ACTION:[IGNORE] REASON:[HF Timeout]"

def run_self_test():
    print(f"🧪 Performing startup self-test with {MODEL}...")
    mock_data = {
        "name": "SELF_TEST_PROCESS",
        "pid": 9999,
        "cpu": 98.5,
        "mean": 12.0,
        "z_score": 15.5,
        "trigger": "SELF_TEST"
    }
    
    response = get_ai_decision(mock_data)
    
    if "ACTION:" in response:
        print(f"✅ AI Connection Verified! Response: {response}")
    else:
        print(f"❌ AI Test Failed. Response: {response}")

def get_chat_response(query, system_status):
    messages = [
        {
            "role": "system",
            "content": f"You are an AI System Assistant for a Linux server. Current System State:\n{system_status}\n\nAnswer the user's questions about the system state or general Linux process management. Keep answers concise."
        },
        {
            "role": "user",
            "content": query
        }
    ]
    
    try:
        completion = client.chat.completions.create(
            model=MODEL,
            messages=messages,
            max_tokens=150,
            temperature=0.7
        )
        return completion.choices[0].message.content.strip()
    except Exception as e:
        return f"Error: {str(e)}"

def start_ai_server():
    run_self_test()

    if os.path.exists(SOCKET_PATH): os.remove(SOCKET_PATH)
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(SOCKET_PATH)
    server.listen(1)

    print(f"🤖 HF Agent ready. Waiting for Daemon...")

    while True:
        try:
            conn, _ = server.accept()
            raw_data = conn.recv(4096).decode() # Increased buffer for chat
            if raw_data:
                request = json.loads(raw_data)
                
                # Mode 1: Interactive Chat
                if request.get("mode") == "chat":
                    print(f"💬 Chat Query: {request.get('query')[:50]}...")
                    response = get_chat_response(request.get("query"), request.get("status"))
                    conn.sendall(response.encode())
                
                # Mode 2: Anomaly Analysis (Default)
                else:
                    print(f"📡 Analyzing PID {request['pid']}...")
                    decision = get_ai_decision(request)
                    print(f"🧠 Result: {decision}")
                    conn.sendall(decision.encode())
                    
            conn.close()
        except KeyboardInterrupt:
            break
        except Exception as e:
            print(f"Server Error: {e}")

if __name__ == "__main__":
    print("--- AI AGENT STARTING ---", flush=True)
    start_ai_server()