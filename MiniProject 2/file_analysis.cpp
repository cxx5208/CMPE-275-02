#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <numeric>
#include <filesystem>
#include <regex>
#include <omp.h>
#include <nlohmann/json.hpp>

using Json = nlohmann::json;
namespace fs = std::filesystem;

struct EnvironmentalData
{
    double lat;
    double lon;
    std::string timeUTC;
    std::string measurand;
    double value;
    std::string measurementUnit;
    double originalValue;
    int airQualityIndex;
    std::string healthImplication;
    std::string monitoringStation;
    std::string monitoringAgency;
    std::string stationCode;
    std::string detailedStationCode;
};

std::string getDateFromUTC(const std::string &utcTimestamp)
{
    std::regex pattern(R"(\d{4}-\d{2}-\d{2})");
    std::smatch results;
    if (std::regex_search(utcTimestamp, results, pattern) && !results.empty())
    {
        return results[0];
    }
    return "";
}

void loadCSVData(const std::string &filepath, std::vector<EnvironmentalData> &dataset)
{
    std::ifstream fileStream(filepath);
    std::string currentLine;
    std::getline(fileStream, currentLine); // Ignore the header

    while (std::getline(fileStream, currentLine))
    {
        std::istringstream stream(currentLine);
        EnvironmentalData dataEntry;

        try
        {
            auto stripQuotes = [](const std::string &input)
            {
                size_t start = input.find_first_not_of('"');
                size_t end = input.find_last_not_of('"');
                return input.substr(start, (end - start + 1));
            };

            std::string valueField;
            std::getline(stream, valueField, ',');
            dataEntry.lat = std::stod(stripQuotes(valueField));
            std::getline(stream, valueField, ',');
            dataEntry.lon = std::stod(stripQuotes(valueField));
            std::getline(stream, dataEntry.timeUTC, ',');
            dataEntry.timeUTC = stripQuotes(dataEntry.timeUTC);
            std::getline(stream, dataEntry.measurand, ',');
            dataEntry.measurand = stripQuotes(dataEntry.measurand);
            std::getline(stream, valueField, ',');
            dataEntry.value = std::stod(stripQuotes(valueField));
            std::getline(stream, dataEntry.measurementUnit, ',');
            dataEntry.measurementUnit = stripQuotes(dataEntry.measurementUnit);
            std::getline(stream, valueField, ',');
            dataEntry.originalValue = std::stod(stripQuotes(valueField));
            std::getline(stream, valueField, ',');
            dataEntry.airQualityIndex = std::stoi(stripQuotes(valueField));
            std::getline(stream, dataEntry.healthImplication, ',');
            dataEntry.healthImplication = stripQuotes(dataEntry.healthImplication);
            std::getline(stream, dataEntry.monitoringStation, ',');
            dataEntry.monitoringStation = stripQuotes(dataEntry.monitoringStation);
            std::getline(stream, dataEntry.monitoringAgency, ',');
            dataEntry.monitoringAgency = stripQuotes(dataEntry.monitoringAgency);
            std::getline(stream, valueField, ',');
            dataEntry.stationCode = stripQuotes(valueField);
            std::getline(stream, valueField);
            dataEntry.detailedStationCode = stripQuotes(valueField);

#pragma omp critical
            dataset.push_back(dataEntry);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing line: " << currentLine << "; Exception: " << e.what() << std::endl;
        }
    }
}

std::map<std::string, double> calculateDailyAverages(const std::vector<EnvironmentalData> &data)
{
    std::map<std::string, std::vector<int>> dailyValues;

#pragma omp parallel for
    for (size_t i = 0; i < data.size(); ++i)
    {
        const auto &entry = data[i];
        std::string day = getDateFromUTC(entry.timeUTC);
        if (!day.empty() && entry.airQualityIndex != -999)
        {
#pragma omp critical
            dailyValues[day].push_back(entry.airQualityIndex);
        }
    }

    std::map<std::string, double> dailyAverages;
    for (const auto &entry : dailyValues)
    {
        dailyAverages[entry.first] = std::accumulate(entry.second.begin(), entry.second.end(), 0.0) / entry.second.size();
    }

    return dailyAverages;
}

std::map<std::string, std::map<int, double>> calculateHourlyAverages(const std::vector<EnvironmentalData> &data)
{
    std::map<std::string, std::map<int, std::vector<int>>> hourlyValues;

#pragma omp parallel for
    for (size_t i = 0; i < data.size(); ++i)
    {
        const auto &entry = data[i];
        std::string day = getDateFromUTC(entry.timeUTC);
        if (!day.empty())
        {
            std::tm timeStruct = {};
            std::istringstream timeStream(entry.timeUTC);
            timeStream >> std::get_time(&timeStruct, "%Y-%m-%dT%H:%M:%SZ");
            int hour = timeStruct.tm_hour;
            if (entry.airQualityIndex != -999)
            {
#pragma omp critical
                hourlyValues[day][hour].push_back(entry.airQualityIndex);
            }
        }
    }

    std::map<std::string, std::map<int, double>> hourlyAverages;
    for (const auto &dayEntry : hourlyValues)
    {
        for (const auto &hourEntry : dayEntry.second)
        {
            hourlyAverages[dayEntry.first][hourEntry.first] = std::accumulate(hourEntry.second.begin(), hourEntry.second.end(), 0.0) / hourEntry.second.size();
        }
    }

    return hourlyAverages;
}

Json exportToJson(const std::map<std::string, double> &dailyAvg, const std::map<std::string, std::map<int, double>> &hourlyAvg)
{
    Json resultJson;
    resultJson["DailyAverages"] = dailyAvg;
    for (const auto &dayEntry : hourlyAvg)
    {
        resultJson["HourlyAverages"][dayEntry.first] = dayEntry.second;
    }
    return resultJson;
}

int main()
{
    auto startTime = std::chrono::high_resolution_clock::now();
    omp_set_num_threads(4);
    std::vector<EnvironmentalData> allData;
    std::string basePath = "cleaned";
    std::vector<std::string> csvFiles;

    for (const auto &dirEntry : fs::directory_iterator(basePath))
    {
        if (dirEntry.is_directory())
        {
            for (const auto &fileEntry : fs::directory_iterator(dirEntry.path()))
            {
                if (fileEntry.is_regular_file() && fileEntry.path().extension() == ".csv")
                {
                    csvFiles.push_back(fileEntry.path().string());
                }
            }
        }
    }

#pragma omp parallel for
    for (size_t i = 0; i < csvFiles.size(); ++i)
    {
        std::vector<EnvironmentalData> tempData;
        loadCSVData(csvFiles[i], tempData);

#pragma omp critical
        allData.insert(allData.end(), tempData.begin(), tempData.end());
    }

    auto dailyAvg = calculateDailyAverages(allData);
    auto hourlyAvg = calculateHourlyAverages(allData);

    Json resultsJson = exportToJson(dailyAvg, hourlyAvg);
    std::ofstream resultsFile("results_analysis.json");
    resultsFile << resultsJson.dump(4);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    std::cout << "Processing time: " << duration.count() << " ms\n";

    return 0;
}
