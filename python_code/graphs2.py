import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from mpl_finance import candlestick_ohlc
import pandas as pd
import datetime

plt.style.use('dark_background')
# Load the candlestick data from the .txt file
df1 = pd.read_csv('BINANCEETHUSDT_c.txt', delim_whitespace=True, header=0)
df2 = pd.read_csv('AAPL_c.txt', delim_whitespace=True, header=0)
df3 = pd.read_csv('AMZN_c.txt', delim_whitespace=True, header=0)
df4 = pd.read_csv('MSFT_c.txt', delim_whitespace=True, header=0)
# Rename columns to match candlestick chart convention
df1.columns = ['High', 'Low', 'Volume', 'Open', 'Close']
df2.columns = ['High', 'Low', 'Volume', 'Open', 'Close']
df3.columns = ['High', 'Low', 'Volume', 'Open', 'Close']
df4.columns = ['High', 'Low', 'Volume', 'Open', 'Close']

# Generate a fake Date index (if you don't have actual dates)
# Generate a Date index that increases by 1 minute for each row
df1['Date'] = pd.date_range(start='2023-09-30 08:45', periods=len(df1), freq='min')
df2['Date'] = pd.date_range(start='2023-09-30 08:45', periods=len(df1), freq='min')
df3['Date'] = pd.date_range(start='2023-09-30 08:45', periods=len(df1), freq='min')
df4['Date'] = pd.date_range(start='2023-09-30 08:45', periods=len(df1), freq='min')


# Convert the Date column to a format matplotlib can work with
df1['Date'] = df1['Date'].map(mdates.date2num)
df2['Date'] = df2['Date'].map(mdates.date2num)
df3['Date'] = df3['Date'].map(mdates.date2num)
df4['Date'] = df4['Date'].map(mdates.date2num)

# Prepare the data in the format: [date, open, high, low, close]
ohlc1 = df1[['Date', 'Open', 'High', 'Low', 'Close']].values
ohlc2 = df2[['Date', 'Open', 'High', 'Low', 'Close']].values
ohlc3 = df3[['Date', 'Open', 'High', 'Low', 'Close']].values
ohlc4 = df4[['Date', 'Open', 'High', 'Low', 'Close']].values

# Create a new figure and axes for the 4 subplots (2x2 grid)
fig, axes = plt.subplots(2, 2, figsize=(12, 8))

# List to hold the axes for easy access
axes_list = [axes[0, 0], axes[0, 1], axes[1, 0], axes[1, 1]]

# Loop over the axes to plot the candlestick chart in each one

# Plot the candlestick chart on the current axis
candlestick_ohlc(axes_list[0], ohlc1, width=0.0006, colorup='green', colordown='red')

# Format the date on the x-axis
axes_list[0].xaxis_date()
axes_list[0].xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d'))

# Rotate date labels for better readability
axes_list[0].tick_params(axis='x', rotation=45)

# Set grid and labels
axes_list[0].grid(True)
axes_list[0].set_xlabel('Date')
axes_list[0].set_ylabel('Price')
axes_list[0].set_title('ETHUSDT')

candlestick_ohlc(axes_list[1], ohlc2, width=0.0006, colorup='green', colordown='red')
# Format the date on the x-axis
axes_list[1].xaxis_date()
axes_list[1].xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d'))

# Rotate date labels for better readability
axes_list[1].tick_params(axis='x', rotation=45)

# Set grid and labels
axes_list[1].grid(True)
axes_list[1].set_xlabel('Date')
axes_list[1].set_ylabel('Price')
axes_list[1].set_title('AAPL')

candlestick_ohlc(axes_list[2], ohlc3, width=0.0006, colorup='green', colordown='red')

# Format the date on the x-axis
axes_list[2].xaxis_date()
axes_list[2].xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d'))

# Rotate date labels for better readability
axes_list[2].tick_params(axis='x', rotation=45)

# Set grid and labels
axes_list[2].grid(True)
axes_list[2].set_xlabel('Date')
axes_list[2].set_ylabel('Price')
axes_list[2].set_title('AMZN')

candlestick_ohlc(axes_list[3], ohlc4, width=0.0006, colorup='green', colordown='red')

# Format the date on the x-axis
axes_list[3].xaxis_date()
axes_list[3].xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d'))

# Rotate date labels for better readability
axes_list[3].tick_params(axis='x', rotation=45)

# Set grid and labels
axes_list[3].grid(True)
axes_list[3].set_xlabel('Date')
axes_list[3].set_ylabel('Price')
axes_list[3].set_title('MSFT')

# Set the title for the figure
plt.suptitle('Candlestick Charts', fontsize=16)

# Adjust the layout for better readability
plt.tight_layout(rect=[0, 0, 1, 0.95])

# Show the plot
plt.show()
