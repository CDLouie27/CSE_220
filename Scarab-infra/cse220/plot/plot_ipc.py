import os
import json
import argparse
import matplotlib
import matplotlib.pyplot as plt
import numpy as np

matplotlib.rc('font', size=14)

def read_descriptor_from_json(descriptor_filename):
    try:
        with open(descriptor_filename, 'r') as json_file:
            descriptor_data = json.load(json_file)
        return descriptor_data
    except FileNotFoundError:
        print(f"Error: File '{descriptor_filename}' not found.")
        return None
    except json.JSONDecodeError as e:
        print(f"Error decoding JSON in file '{descriptor_filename}': {e}")
        return None

def calculate_accuracy(sim_path, descriptor_data):
    benchmarks = descriptor_data["workloads_list"]
    configurations = descriptor_data["configurations"]
    experiment = descriptor_data["experiment"]
    accuracy = {}

    for config_key in configurations.keys():
        accuracy[config_key] = []
        for benchmark in benchmarks:
            bp_file_path = os.path.join(sim_path, benchmark, experiment, config_key, "bp.stat.0.csv")
            try:
                br_correct = 0
                br_recover = 0
                with open(bp_file_path, "r") as bp_file:
                    for line in bp_file:
                        if "BR_CORRECT" in line:
                            br_correct = float(line.split(",")[1].strip())
                        elif "BR_RECOVER" in line:
                            br_recover = float(line.split(",")[1].strip())
                
                # Calculate accuracy
                total = br_correct + br_recover
                acc = br_correct / total if total > 0 else 0
                accuracy[config_key].append(acc)
            except FileNotFoundError:
                print(f"Error: File '{bp_file_path}' not found.")
                accuracy[config_key].append(0)
            except Exception as e:
                print(f"Error processing file '{bp_file_path}': {e}")
                accuracy[config_key].append(0)

    return benchmarks, accuracy

def plot_accuracy(benchmarks, accuracy, output_dir):
    colors = ['#800000', '#911eb4', '#4363d8', '#f58231', '#3cb44b', '#46f0f0', '#f032e6', '#bcf60c', '#fabebe', '#e6beff', '#e6194b', '#000075']
    ind = np.arange(len(benchmarks))
    width = 0.15

    fig, ax = plt.subplots(figsize=(14, 4.4), dpi=80)

    for idx, (config, values) in enumerate(accuracy.items()):
        ax.bar(ind + idx * width, values, width=width, color=colors[idx % len(colors)], label=config)

    ax.set_xlabel("Benchmarks")
    ax.set_ylabel("Branch Prediction Accuracy")
    ax.set_xticks(ind + width * (len(accuracy) - 1) / 2)
    ax.set_xticklabels(benchmarks, rotation=27, ha='right')
    ax.grid('y')
    ax.set_ylim(0, 1.0)
    ax.legend(loc="upper left", ncols=2)
    fig.tight_layout()

    plot_path = os.path.join(output_dir, "Branch_Prediction_Accuracy.png")
    plt.savefig(plot_path, format="png", bbox_inches="tight")
    plt.show()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Read descriptor file and calculate branch prediction accuracy')
    parser.add_argument('-o', '--output_dir', required=True, help='Output path. Usage: -o /home/$USER/plot')
    parser.add_argument('-d', '--descriptor_name', required=True, help='Experiment descriptor name. Usage: -d /home/$USER/lab1.json')
    parser.add_argument('-s', '--simulation_path', required=True, help='Simulation result path. Usage: -s /home/$USER/exp/simulations')

    args = parser.parse_args()

    descriptor_data = read_descriptor_from_json(args.descriptor_name)
    if descriptor_data:
        benchmarks, accuracy = calculate_accuracy(args.simulation_path, descriptor_data)
        plot_accuracy(benchmarks, accuracy, args.output_dir)
