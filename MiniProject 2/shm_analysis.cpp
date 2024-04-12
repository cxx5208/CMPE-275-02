#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <numeric>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <regex>
#include <mpi.h>
#include <omp.h>
#include <nlohmann/json.hpp>

using JsonDocument = nlohmann::json;

const size_t MAX_AQI_RECORDS = 1000000000;

struct AirQualityData
{
    double lat;
    double lon;
    char timestamp[20];
    char pollutant[20];
    double measurement;
    char measurementUnit[10];
    double originalMeasurement;
    int index;
    char riskLevel[2];
    char monitoringStation[100];
    char monitoringAgency[100];
    char stationCode[20];
    char detailedStationCode[20];
};

struct MemoryArea
{
    size_t recordCount;
    AirQualityData records[MAX_AQI_RECORDS];
};

MemoryArea *accessMemory(const std::string &segmentName, size_t size)
{
    int shm_fd = shm_open(segmentName.c_str(), O_RDONLY, 0666);
    if (shm_fd < 0)
    {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    void *address = mmap(nullptr, size, PROT_READ, MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    if (address == MAP_FAILED)
    {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    return reinterpret_cast<MemoryArea *>(address);
}

std::string getDateFromTimestamp(const std::string &utc)
{
    std::regex pattern(R"(\d{4}-\d{2}-\d{2})");
    std::smatch results;
    if (std::regex_search(utc, results, pattern) && !results.empty())
    {
        return results[0];
    }
    return "";
}

std::map<std::string, double> computeDailyAverages(MemoryArea *memory)
{
    std::map<std::string, std::vector<int>> dailyValues;
    std::map<std::string, double> dailyAverages;

#pragma omp parallel
    {
        std::map<std::string, std::vector<int>> privateValues;

#pragma omp for nowait
        for (size_t i = 0; i < memory->recordCount; ++i)
        {
            std::string date = getDateFromTimestamp(memory->records[i].timestamp);
            if (!date.empty() && memory->records[i].index != -999)
            {
#pragma omp critical
                privateValues[date].push_back(memory->records[i].index);
            }
        }

#pragma omp critical
        for (auto &pair : privateValues)
        {
            dailyValues[pair.first].insert(dailyValues[pair.first].end(), pair.second.begin(), pair.second.end());
        }
    }

    for (const auto &pair : dailyValues)
    {
        dailyAverages[pair.first] = std::accumulate(pair.second.begin(), pair.second.end(), 0.0) / pair.second.size();
    }

    return dailyAverages;
}

std::map<std::string, std::map<int, double>> computeHourlyAverages(MemoryArea *memory)
{
    std::map<std::string, std::map<int, std::vector<int>>> hourlyData;
    std::map<std::string, std::map<int, double>> hourlyAverages;

#pragma omp parallel
    {
        std::map<std::string, std::map<int, std::vector<int>>> privateData;

#pragma omp for nowait
        for (size_t i = 0; i < memory->recordCount; ++i)
        {
            std::string day = getDateFromTimestamp(memory->records[i].timestamp);
            if (!day.empty())
            {
                std::istringstream iss(std::string(memory->records[i].timestamp + 11, 2));
                int hour;
                if (iss >> hour && memory->records[i].index != -999)
                {
                    privateData[day][hour].push_back(memory->records[i].index);
                }
            }
        }

#pragma omp critical
        for (auto &dayData : privateData)
        {
            for (auto &hourData : dayData.second)
            {
                hourlyData[dayData.first][hourData.first].insert(
                    hourlyData[dayData.first][hourData.first].end(),
                    hourData.second.begin(), hourData.second.end());
            }
        }
    }

    for (const auto &dayData : hourlyData)
    {
        for (const auto &hourData : dayData.second)
        {
            double average = std::accumulate(hourData.second.begin(), hourData.second.end(), 0.0) / hourData.second.size();
            hourlyAverages[dayData.first][hourData.first] = average;
        }
    }

    return hourlyAverages;
}

JsonDocument formatResults(const std::map<std::string, double> &dailyAvg, const std::map<std::string, std::map<int, double>> &hourlyAvg)
{
    JsonDocument document;
    document["dailyAvg"] = dailyAvg;
    for (const auto &dayData : hourlyAvg)
    {
        document["hourlyAvg"][dayData.first] = dayData.second;
    }
    return document;
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0)
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        MemoryArea *memory = accessMemory("/aqi_segment", sizeof(MemoryArea));

        auto dailyAvg = computeDailyAverages(memory);
        auto hourlyAvg = computeHourlyAverages(memory);
        auto end_time = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::cout << "Time to analyze: " << duration.count() << " milliseconds." << std::endl;

        munmap(memory, sizeof(MemoryArea));

        JsonDocument results = formatResults(dailyAvg, hourlyAvg);
        std::string resultsString = results.dump();

        auto currentTime = std::chrono::system_clock::now();
        auto millisSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()).count();
        std::cout << "Current time (ms since epoch): " << millisSinceEpoch << std::endl;

        MPI_Send(resultsString.c_str(), resultsString.size() + 1, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}
