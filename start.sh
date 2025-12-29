#!/bin/bash

# Process Sentinel - Unified Launcher

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}=========================================${NC}"
echo -e "${BLUE}      PROCESS SENTINEL LAUNCHER          ${NC}"
echo -e "${BLUE}=========================================${NC}"

# 1. Check Prereqs
if [ -z "$HF_TOKEN" ]; then
    echo -e "${RED}Error: HF_TOKEN is not set.${NC}"
    echo "Please export your Hugging Face token:"
    echo "  export HF_TOKEN='your_token_here'"
    exit 1
fi

# 2. Build & Clean
echo -e "${GREEN}[+] Cleaning and Building...${NC}"
make clean > /dev/null 2>&1
make ipc-clean > /dev/null 2>&1
make > /dev/null
if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

# 3. Start AI Agent (Background)
echo -e "${GREEN}[+] Starting AI Agent...${NC}"
python3 -u ai/ai_agent.py > ai_agent.log 2>&1 &
AI_PID=$!
echo "    -> AI Agent PID: $AI_PID (Logs: ai_agent.log)"

# 4. Start Daemon (Background)
echo -e "${GREEN}[+] Starting Sentinel Daemon...${NC}"
./sentinel_daemon > daemon.log 2>&1 &
DAEMON_PID=$!
    echo "    -> Daemon PID: $DAEMON_PID (Logs: daemon.log)"

# Give them a moment to initialize
sleep 2

# 5. Auto-Add AI Agent to Monitor (So dashboard isn't empty)
echo -e "${GREEN}[+] Adding AI Agent to Monitor...${NC}"
./sentinel_cli --add $AI_PID

# 6. Trap Ctrl+C to cleanup
cleanup() {    echo -e "\n${RED}[!] Shutting down...${NC}"
    kill $AI_PID 2>/dev/null
    kill $DAEMON_PID 2>/dev/null
    ./sentinel_cli --shutdown > /dev/null 2>&1
    make ipc-clean > /dev/null 2>&1
    exit
}
trap cleanup SIGINT SIGTERM

# 6. Launch Dashboard
echo -e "${GREEN}[+] Launching Dashboard...${NC}"
./sentinel_cli --monitor

# If dashboard exits, cleanup
cleanup
