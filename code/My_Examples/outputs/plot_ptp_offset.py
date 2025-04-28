import pandas as pd
import matplotlib.pyplot as plt

# Load the CSV file
csv_path = "build/ptp_offset_history.csv"
df = pd.read_csv(csv_path)

# Extract offset columns (ending with '_offset_ns')
offset_columns = [col for col in df.columns if col.endswith('_offset_ns')]

# Create plot
plt.figure(figsize=(12, 6))

for col in offset_columns:
    cam_id = col.replace('_offset_ns', '')  # Clean label
    plt.plot(df['Sample'], df[col], label=cam_id, linewidth=2)

# Add PTP offset threshold (edit this if needed)
ptp_threshold_ns = 1000 # 1 us
plt.axhline(y=ptp_threshold_ns, color='red', linestyle='--', label='Threshold')

# Labels and layout
plt.title("PTP Offsets per Camera Over Time")
plt.xlabel("Sample Index")
plt.ylabel("Offset from Master (ns)")
plt.legend()
plt.grid(True)
plt.tight_layout()

# Save & show
plt.savefig("ptp_offsets_plot.png")
plt.show()
