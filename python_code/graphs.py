import pandas as pd
import matplotlib.pyplot as plt

plt.style.use('dark_background')
# Load the data into a DataFrame
df1 = pd.read_csv('BINANCEETHUSDT.txt', delim_whitespace=True)
df2 = pd.read_csv('AAPL.txt', delim_whitespace=True)
df3 = pd.read_csv('AMZN.txt', delim_whitespace=True)
df4 = pd.read_csv('MSFT.txt', delim_whitespace=True)
# Strip any leading or trailing whitespace from the column names
df1.columns = df1.columns.str.strip()
df2.columns = df2.columns.str.strip()
df3.columns = df3.columns.str.strip()
df4.columns = df4.columns.str.strip()
# Rename columns for easier access
df1.columns = ['price', 'server_time', 'volume', 'time_received', 'time_consumed']
df2.columns = ['price', 'server_time', 'volume', 'time_received', 'time_consumed']
df3.columns = ['price', 'server_time', 'volume', 'time_received', 'time_consumed']
df4.columns = ['price', 'server_time', 'volume', 'time_received', 'time_consumed']

# Calculate the delay between time_received and server_time
df1['delay'] = df1['time_received'] - df1['server_time']/1000
df2['delay'] = df2['time_received'] - df2['server_time']/1000
df3['delay'] = df3['time_received'] - df3['server_time']/1000
df4['delay'] = df4['time_received'] - df4['server_time']/1000





fig, axes = plt.subplots(2, 2, figsize=(12, 8))

# Solid line with circle markers (Top-left subplot)
axes[0, 0].plot(df1['delay'], marker='o', linestyle='-', color='blue', linewidth=0.1, markersize=1)
axes[0, 0].set_title('Server Delay ETHUSDT')
axes[0, 0].set_xlabel('# of trades')
axes[0, 0].set_ylabel('Delay (Seconds)')
axes[0, 0].grid(True)

# Dashed line with square markers (Top-right subplot)
axes[0, 1].plot(df2['delay'], marker='o', linestyle='--', color='red', linewidth=0.1, markersize=1)
axes[0, 1].set_title('Server Delay AAPL')
axes[0, 1].set_xlabel('# of trades')
axes[0, 1].set_ylabel('Delay (Seconds)')
axes[0, 1].grid(True)

# Dash-dot line with triangle-up markers (Bottom-left subplot)
axes[1, 0].plot(df3['delay'], marker='o', linestyle='-.', color='green', linewidth=0.1, markersize=1)
axes[1, 0].set_title('Server delay AMZN')
axes[1, 0].set_xlabel('# of trades')
axes[1, 0].set_ylabel('Delay (Seconds)')
axes[1, 0].grid(True)

# Dotted line with diamond markers (Bottom-right subplot)
axes[1, 1].plot(df4['delay'], marker='o', linestyle=':', color='purple', linewidth=0.1, markersize=1)
axes[1, 1].set_title('Server Delay MSFT')
axes[1, 1].set_xlabel('# of trades')
axes[1, 1].set_ylabel('Delay (Seconds)')
axes[1, 1].grid(True)

# Adjust layout
plt.tight_layout()

# Show plot
plt.show()


