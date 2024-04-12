import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime
from mpi4py import MPI
import json
import time

def display_daily_aqi(aqi_data):
    dates_list = [datetime.strptime(day, "%Y-%m-%d") for day in aqi_data.keys()]
    aqi_scores = list(aqi_data.values())

    plt.figure(figsize=(12, 6))
    plt.plot(dates_list, aqi_scores, marker='o', linestyle='-', color='blue')
    plt.title('Daily AQI Averages')
    plt.xlabel('Date')
    plt.ylabel('AQI Value')
    plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d'))
    plt.gca().xaxis.set_major_locator(mdates.DayLocator(interval=1))
    plt.gcf().autofmt_xdate()
    plt.grid(True)
    plt.show()

def display_hourly_aqi(aqi_hourly, selected_date):
    if selected_date not in aqi_hourly:
        print(f"Data unavailable for {selected_date}")
        return

    hourly_info = aqi_hourly[selected_date][:10]
    print(f"Sample hourly data for {selected_date}:")
    for data in hourly_info:
        hour, aqi = data
        print(f"Hour: {hour}, AQI: {aqi}")

    hours_list, aqi_list = zip(*hourly_info)

    plt.figure(figsize=(12, 6))
    plt.plot(hours_list, aqi_list, marker='o', linestyle='-', color='red')
    plt.title(f'Hourly AQI on {selected_date}')
    plt.xlabel('Hour')
    plt.ylabel('AQI')
    plt.xticks(range(len(hours_list)), [f"{hr}:00" for hr in hours_list], rotation=90)
    plt.grid(True)
    plt.show()

def execute_plotting():
    mpi_comm = MPI.COMM_WORLD
    process_rank = mpi_comm.Get_rank()

    if process_rank == 1:  # Rank 1 handles the plotting
        buffer_size = 100000
        buffer = bytearray(buffer_size)
        mpi_comm.Recv(buffer, source=0, tag=0)  # Receive from rank 0 (C++ process)

        json_str = buffer.decode('utf-8').rstrip('\x00')
        aqi_results = json.loads(json_str)
        current_millis = int(time.time() * 1000)
        print(current_millis)

        display_daily_aqi(aqi_results["dailyAverageAQI"])

        date_to_plot = "2020-08-14"
        display_hourly_aqi(aqi_results["hourlyAverageAQI"], date_to_plot)

if __name__ == "__main__":
    execute_plotting()
