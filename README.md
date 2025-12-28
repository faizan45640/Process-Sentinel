# Process Sentinel

Process Sentinel is an AI-powered process monitoring tool for Linux. It watches the resource usage of specified processes and uses a large language model (LLM) to recommend actions when anomalous behavior is detected.

This project consists of three main components:
1.  **Sentinel Daemon (`sentinel_daemon`)**: The background service that manages monitoring threads.
2.  **AI Agent (`ai/ai_agent.py`)**: A Python script that communicates with the Hugging Face API to get AI-driven advice.
3.  **Sentinel CLI (`sentinel_cli`)**: A command-line interface to control the daemon and view the monitoring dashboard.

## Features

- **Real-time Monitoring**: A live dashboard to view the status of all monitored processes.
- **AI-Powered Anomaly Detection**: Uses statistical analysis (Z-score) to detect anomalies and an LLM to suggest actions.
- **Process Control**: Can automatically (or be configured to) kill, pause, or renice misbehaving processes.
- **Monitor by PID or Name**: Add processes to the watchlist easily by their process ID or name.

## Prerequisites

- A C compiler (like `gcc`)
- `make`
- Python 3
- A Hugging Face account and API token

**IMPORTANT**: This project is designed for **Linux only**. It relies on the `/proc` filesystem and UNIX domain sockets, which are not available on Windows.

## Getting Started

### 1. Set Up the AI Agent

The AI agent requires a Hugging Face API token to make requests to the language model.

1.  Get your token from your [Hugging Face account settings](https://huggingface.co/settings/tokens).
2.  Set it as an environment variable:
    ```bash
    export HF_TOKEN="your_huggingface_token_here"
    ```
    You may want to add this line to your `.bashrc` or `.zshrc` file to make it permanent.

### 2. Build the Project

A `Makefile` is provided to easily build the C components.

```bash
make
```

This will create two executables in the root directory: `sentinel_daemon` and `sentinel_cli`.

## How to Run

You need to run the AI agent and the daemon in separate terminal windows.

**Terminal 1: Start the AI Agent**
```bash
make run-ai
```
or
```bash
python3 ai/ai_agent.py
```

**Terminal 2: Start the Sentinel Daemon**
```bash
make run-daemon
```
or
```bash
./sentinel_daemon
```

**Terminal 3: Use the CLI**

Once the daemon is running, you can use the CLI to manage it.

## CLI Commands

- **`./sentinel_cli --monitor`**
  Open the real-time monitoring dashboard.

- **`./sentinel_cli --add <pid>`**
  Start monitoring a process by its PID.
  *Example: `./sentinel_cli --add 1234`*

- **`./sentinel_cli --add-by-name <name>`**
  Start monitoring a process by its name.
  *Example: `./sentinel_cli --add-by-name firefox`*

- **`./sentinel_cli --remove <pid>`**
  Stop monitoring a process.
  *Example: `./sentinel_cli --remove 1234`*

- **`./sentinel_cli --shutdown`**
  Signal the Sentinel daemon to shut down gracefully.

## Cleaning Up

To remove the compiled executables and object files:
```bash
make clean
```