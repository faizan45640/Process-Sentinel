import time
import math
import os

def waste_cpu(duration):
    """ Spins the CPU to generate load """
    end_time = time.time() + duration
    while time.time() < end_time:
        math.sqrt(12345.6789)

def relax(duration):
    """ Sleeps to lower average CPU usage """
    time.sleep(duration)

if __name__ == "__main__":
    pid = os.getpid()
    print(f"Stress Test Running! PID: {pid}")
    print("Step 1: Establishing baseline (Low CPU)...")
    
    # 1. Establish a low baseline so the spike is obvious
    for i in range(10):
        print(f"[{i+1}/10] Sleeping (Low Load)...")
        relax(1)

    print("\nStep 2: Triggering ANOMALY (High CPU)...")
    # 2. Spike the CPU hard
    try:
        while True:
            waste_cpu(0.5)
    except KeyboardInterrupt:
        print("\nTest stopped.")
