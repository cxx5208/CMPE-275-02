#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <clocale>
#include <omp.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <chrono>
#include <locale.h>

namespace fs = std::filesystem;

const size_t MAX_ENTRIES = 10000000000; // Tailored to the anticipated data volume

struct EnvironmentalData
{
    double lat;
    double lon;
    char timeUTC[20];
    char pollutant[20];
    double concentrationLevel;
    char units[10];
    double originalConcentration;
    int airQualityIndex;
    char riskCategory[50];
    char observationStation[100];
    char regulatoryAgency[100];
    char stationIdentifier[20];
    char fullIdentifier[20];
};

struct MemoryMap
{
    size_t entryCount;
    EnvironmentalData entries[MAX_ENTRIES];
};

std::string sanitize(const std::string &inputStr)
{
    std::string result;
    std::copy_if(inputStr.begin(), inputStr.end(), std::back_inserter(result), [](unsigned char ch)
                 { return !std::isspace(ch); });
    if (!result.empty() && result.front() == '"')
        result.erase(result.begin());
    if (!result.empty() && result.back() == '"')
        result.pop_back();
    return result;
}

size_t estimateMemoryNeeds(const std::string &file)
{
    std::ifstream stream(file);
    std::string line;
    size_t lineCount = 0;

    while (std::getline(stream, line))
    {
        ++lineCount;
    }

    return sizeof(MemoryMap) + (lineCount * sizeof(EnvironmentalData));
}

MemoryMap *createSharedMemory(const std::string &segmentName, size_t size)
{
    int shmFd = shm_open(segmentName.c_str(), O_CREAT | O_RDWR, 0666);
    if (shmFd < 0)
    {
        perror("Failed to open shared memory");
        exit(EXIT_FAILURE);
    }
    if (ftruncate(shmFd, size) < 0)
    {
        perror("Failed to truncate shared memory");
        close(shmFd);
        exit(EXIT_FAILURE);
    }
    void *mapped = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    close(shmFd);
    if (mapped == MAP_FAILED)
    {
        perror("Failed to map shared memory");
        exit(EXIT_FAILURE);
    }
    return reinterpret_cast<MemoryMap *>(mapped);
}

std::vector<EnvironmentalData> readCSVData(const std::string &path)
{
    std::ifstream file(path);
    std::string line;
    std::vector<EnvironmentalData> data;

    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string piece;
        EnvironmentalData info;

        std::getline(ss, piece, ',');
        info.lat = std::stod(sanitize(piece));

        std::getline(ss, piece, ',');
        info.lon = std::stod(sanitize(piece));

        std::getline(ss, piece, ',');
        strncpy(info.timeUTC, sanitize(piece).c_str(), sizeof(info.timeUTC));

        std::getline(ss, piece, ',');
        strncpy(info.pollutant, sanitize(piece).c_str(), sizeof(info.pollutant));

        std::getline(ss, piece, ',');
        info.concentrationLevel = std::stod(sanitize(piece));

        std::getline(ss, piece, ',');
        strncpy(info.units, sanitize(piece).c_str(), sizeof(info.units));

        std::getline(ss, piece, ',');
        info.originalConcentration = std::stod(sanitize(piece));

        std::getline(ss, piece, ',');
        info.airQualityIndex = std::stoi(sanitize(piece));

        std::getline(ss, piece, ',');
        strncpy(info.riskCategory, sanitize(piece).c_str(), sizeof(info.riskCategory));

        std::getline(ss, piece, ',');
        strncpy(info.observationStation, sanitize(piece).c_str(), sizeof(info.observationStation));

        std::getline(ss, piece, ',');
        strncpy(info.regulatoryAgency, sanitize(piece).c_str(), sizeof(info.regulatoryAgency));

        std::getline(ss, piece, ',');
        strncpy(info.stationIdentifier, sanitize(piece).c_str(), sizeof(info.stationIdentifier));

        std::getline(ss, piece, ',');
        strncpy(info.fullIdentifier, sanitize(piece).c_str(), sizeof(info.fullIdentifier));

        data.push_back(info);
    }
    return data;
}

void handleCSVsInDirectory(const fs::path &directory, MemoryMap *memMap)
{
    omp_set_num_threads(4);
    std::vector<fs::path> csvFiles;
    for (const auto &entry : fs::recursive_directory_iterator(directory))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".csv")
        {
            csvFiles.push_back(entry.path());
        }
    }

    std::vector<std::vector<EnvironmentalData>> allData(csvFiles.size());

#pragma omp parallel for
    for (size_t i = 0; i < csvFiles.size(); ++i)
    {
        allData[i] = readCSVData(csvFiles[i].string());
    }

    // Flatten all data into one vector
    std::vector<EnvironmentalData> combinedData;
    for (const auto &batch : allData)
    {
        combinedData.insert(combinedData.end(), batch.begin(), batch.end());
    }

    // Store in shared memory
    if (combinedData.size() <= MAX_ENTRIES)
    {
        std::copy(combinedData.begin(), combinedData.end(), memMap->entries);
        memMap->entryCount = combinedData.size();
    }
    else
    {
        std::cerr << "Insufficient space in shared memory for all entries." << std::endl;
    }
}

int main()
{
    setlocale(LC_NUMERIC, "C");
    fs::path dataPath = "path_to_data_directory";
    size_t neededMemory = estimateMemoryNeeds(dataPath);

    std::cout << "Estimated memory requirement: " << neededMemory << " bytes" << std::endl;

    MemoryMap *sharedMemory = createSharedMemory("/env_data_segment", sizeof(MemoryMap));
    if (!sharedMemory)
    {
        std::cerr << "Initialization of shared memory failed." << std::endl;
        return 1;
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    handleCSVsInDirectory(dataPath, sharedMemory);
    auto endTime = std::chrono::high_resolution_clock::now();
    auto processingTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "Processed " << sharedMemory->entryCount << " entries in " << processingTime.count() << " milliseconds." << std::endl;
    std::cout << "Memory Map size: " << sizeof(MemoryMap) << " bytes." << std::endl;

    if (munmap(sharedMemory, sizeof(MemoryMap)) == -1)
    {
        std::cerr << "Error unmapping shared memory." << std::endl;
    }

    return 0;
}
