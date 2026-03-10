import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

try:
    df = pd.read_csv("../build/benchmark_results.csv", sep=",")
except FileNotFoundError:
    print("File not found")
    exit()

plt.figure(figsize=(10,6))
plt.xscale('log')
plt.yscale('log')

plt.title("Average Math Time (CPU)")
plt.xlabel("Agent Count (Log)")
plt.ylabel("Math time (m/s) (Log Scale)")
plt.plot(df['AgentCount'], df["AverageMathTimeMS"], 'o-')
plt.grid(True, which="both", ls="-", alpha=0.2)

for i,row in df.iterrows():
    label = f"{row["AverageMathTimeMS"]:.3f} m/s"
    print(label)
    plt.annotate(label, (row["AgentCount"], row["AverageMathTimeMS"]), textcoords="offset points", xytext=(0, 6), ha='center', fontsize=8)

plt.legend()
plt.show()
