import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime
import json

def visualize_daily_air_quality(daily_data):
    date_keys = [datetime.strptime(key, "%Y-%m-%d") for key in daily_data.keys()]
    aqi_scores = list(daily_data.values())

    plt.figure(figsize=(12, 6))
    plt.plot(date_keys, aqi_scores, marker='o', linestyle='-', color='blue')
    plt.title('Daily Air Quality Index (AQI)')
    plt.xlabel('Date')
    plt.ylabel('AQI Score')
    plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d'))
    plt.gca().xaxis.set_major_locator(mdates.DayLocator(interval=1))
    plt.gcf().autofmt_xdate()  # Auto-format date labels
    plt.grid(True)
    plt.show()

def visualize_hourly_air_quality(hourly_data, selected_date):
    if selected_date not in hourly_data:
        print(f"No data available for {selected_date}")
        return

    # Display the first 10 entries of hourly data for the specified date
    first_ten_hours = hourly_data[selected_date][:10]
    print(f"Data for {selected_date}:")
    for entry in first_ten_hours:
        print(f"Hour: {entry[0]}, AQI: {entry[1]}")

    hours, aqi_values = zip(*first_ten_hours)

    plt.figure(figsize=(12, 6))
    plt.plot(hours, aqi_values, marker='o', linestyle='-', color='red')
    plt.title(f'Hourly AQI on {selected_date}')
    plt.xlabel('Hour of the Day')
    plt.ylabel('AQI Score')
    plt.xticks(range(len(hours)), [f"{hour}:00" for hour in hours], rotation=90)
    plt.grid(True)
    plt.show()

def process_data_and_plot():
    begin_time = datetime.now()
    # Load JSON data file
    with open("analysis_results.json", "r") as file:
        aqi_results = json.load(file)
    end_time = datetime.now()

    elapsed_time = end_time - begin_time
    print("Data Processing Duration:", elapsed_time)

    # Visualize the daily and hourly AQI data
    visualize_daily_air_quality(aqi_results["dailyAverageAQI"])
    
    # Visualize hourly AQI for a given date
    date_of_interest = "2020-08-14"  # Example date
    visualize_hourly_air_quality(aqi_results["hourlyAverageAQI"], date_of_interest)

if __name__ == "__main__":
    process_data_and_plot()
