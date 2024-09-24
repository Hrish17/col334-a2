import json
import subprocess
import time
import matplotlib.pyplot as plt
import signal
import os

# Path to the config file
config_file_path = "config.json"

def modify_p_in_config(p_value):
    """Modify the 'p' value in the config file."""
    with open(config_file_path, "r") as file:
        config = json.load(file)

    config["p"] = p_value

    with open(config_file_path, "w") as file:
        json.dump(config, file, indent=4)

def run_experiment():
    server_process = subprocess.Popen(["./server"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(2)  # Wait for the server to start

    subprocess.run(["./client"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    # Terminate the server after the run
    os.kill(server_process.pid, signal.SIGTERM)

def main():
    """Main function to modify config, run experiments, and plot the results."""
    p_values = list(range(1, 11))
    completion_times = []
    subprocess.run(['make', 'build'], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)

    for p in p_values:
        with open('time.txt', 'w') as f:
            f.write('')

        # Modify the 'p' value in config
        modify_p_in_config(p)

        # Run the server-client experiment and record the completion time
        print(f"Running experiment for p = {p}...")

        # Run the experiment 10 times for each p
        for i in range(10):
            try:
                run_experiment()
            except Exception as e:
                print(f"Error during experiment: {e}")
                continue

        total_time = 0.0000
        with open("time.txt", 'r') as file:
            for i, line in enumerate(file):
                if i >= 10:  # Stop after reading 10 lines
                    break
                try:
                    total_time += float(line)
                except (IndexError, ValueError):
                    print(f"Skipping line {i + 1}: {line.strip()}")
        
        completion_times.append(total_time/10.0000)

    # Plot the results
    plt.figure(figsize=(8, 6))
    plt.plot(p_values, completion_times, marker='o', color='b', label='Average Completion Time')
    plt.xlabel("p (words per packet)")
    plt.ylabel("Completion Time (seconds)")
    plt.title("Completion Time vs Words per Packet (p)")
    plt.grid(True)
    plt.legend()
    plt.savefig("plot.png")
    plt.show()

if __name__ == "__main__":
    main()
