import time
import math
import os
import sys

def waste_cpu():
    """ Spins the CPU to generate 100% load """
    while True:
        math.sqrt(12345.6789)

if __name__ == "__main__":
    pid = os.getpid()
    print(f"========================================")
    print(f"🔥 EXTREME STRESS TEST (PID: {pid})")
    print(f"========================================")
    print("Step 1: CALM PHASE (0% CPU)")
    print("Waiting 15 seconds for you to add the PID...")
    print(f"👉 RUN THIS NOW:  ./sentinel_cli --add {pid}")
    
    # Countdown
    for i in range(15, 0, -1):
        print(f"Spike in {i}s...", end='\r')
        time.sleep(1)
    
    print("\n\nStep 2: ATTACK PHASE (100% CPU)")
    print("🚀 SPIKING CPU NOW! Watch the dashboard!")
    
    try:
        # Launch multiple threads? No, Sentinel monitors total process usage.
        # Single thread 100% is enough to trigger anomaly if previous was 0%.
        waste_cpu()
    except KeyboardInterrupt:
        print("\nTest stopped.")
