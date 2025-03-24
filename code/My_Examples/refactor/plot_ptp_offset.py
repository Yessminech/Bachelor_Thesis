import pandas as pd
import matplotlib.pyplot as plt

# Load the CSV file
df = pd.read_csv("ptp_offset_history.csv")

# Plot the data
plt.figure(figsize=(12, 6))
for column in df.columns[1:]:  # Skip 'Sample' column
    plt.plot(df['Sample'], df[column], label=column, linewidth=2)

# Threshold line (you can hardcode or pass dynamically)
ptp_offset_threshold_ns = 100  # Replace with your actual threshold
plt.axhline(y=ptp_offset_threshold_ns, color='red', linestyle='--', label='Threshold')

# Labels and title
plt.title("PTP Offset Synchronization Over Time")
plt.xlabel("Sample Index (Time)")
plt.ylabel("Offset from Master (ns)")
plt.legend()
plt.grid(True)
plt.tight_layout()

# Save and show the plot
plt.savefig("ptp_offset_plot.png")
plt.show()

## pip install pandas matplotlib
