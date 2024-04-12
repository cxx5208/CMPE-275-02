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

const size_t MAX_RECORD_COUNT = 10000000000; // Set according to expected dataset size

struct AirQualityData
{
    double lat;
    double lon;
    char timestamp[20];
    char measure[20];
    double level;
    char measurementUnits[10];
    double baseLevel;
    int index;
    char qualityCategory[50];
    char stationName[100];
    char agencyName[100];
    char stationID[20];
    char completeStationID[20];
};

struct MemoryBlock
{
    size_t totalRecords;
    AirQualityData records[MAX_RECORD_COUNT];
};

std::string cleanWhitespace(const std::string &input)
{
    std::string output;
    std::copy_if(input.begin(), input.end(), std::back_inserter(output), [](unsigned char c)
                 { return !std::isspace(c); });
    if (!output.empty() && output.front() == '"')
        output.erase(output.begin());
    if (!output.empty() && output.back() == '"')
        output.pop_back();
    return output;
}

size_t getMemorySizeForCSV(const std::string &file)
{
    std::ifstream stream(file);
    std::string line;
    size_t count = 0;

    while (std::getline(stream, line))
    {
        ++count; // Count each line
    }

    return sizeof(MemoryBlock) + (count * sizeof(AirQualityData));
}

MemoryBlock *setupMemory(const std::string &identifier, size_t size)
{
    int shmDescriptor = shm_open(identifier.c_str(), O_CREAT | O_RDWR, 0666);
    if (shmDescriptor < 0)
    {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    if (ftruncate(shmDescriptor, size) < 0)
    {
        perror("ftruncate");
        close(shmDescriptor);
        exit(EXIT_FAILURE);
    }
    void *ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, shmDescriptor, 0);
    close(shmDescriptor);
    if (ptr == MAP_FAILED)
    {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    return reinterpret_cast<MemoryBlock *>(ptr);
}

void readCSV(const std::string &path, MemoryBlock *mem)
{
    std::ifstream file(path);
    std::string line;
    size_t current = mem->totalRecords;

    while (std::getline(file, line) && current < MAX_RECORD_COUNT)
    {
        std::stringstream ss(line);
        std::string part;
        AirQualityData data;

        std::getline(ss, part, ',');
        data.lat = std::stod(cleanWhitespace(part));

        std::getline(ss, part, ',');
        data.lon = std::stod(cleanWhitespace(part));

        std::getline(ss, part, ',');
        strncpy(data.timestamp, cleanWhitespace(part).c_str(), sizeof(data.timestamp));

        std::getline(ss, part, ',');
        strncpy(data.measure, cleanWhitespace(part).c_str(), sizeof(data.measure));

        std::getline(ss, part, ',');
        data.level = std::stod(cleanWhitespace(part));

        std::getline(ss, part, ',');
        strncpy(data.measurementUnits, cleanWhitespace(part).c_str(), sizeof(data.measurementUnits));

        std::getline(ss, part, ',');
        data.baseLevel = std::stod(cleanWhitespace(part));

        std::getline(ss, part, ',');
        data.index = std::stoi(cleanWhitespace(part));

        std::getline(ss, part, ',');
        strncpy(data.qualityCategory, cleanWhitespace(part).c_str(), sizeof(data.qualityCategory));

        std::getline(ss, part, ',');
        strncpy(data.stationName, cleanWhitespace(part).c_str(), sizeof(data.stationName));

        std::getline(ss, part, ',');
        strncpy(data.agencyName, cleanWhitespace(part).c_str(), sizeof(data.agencyName));

        std::getline(ss, part, ',');
        strncpy(data.stationID, cleanWhitespace(part).c_str(), sizeof(data.stationID));

        std::getline(ss, part, ',');
        strncpy(data.completeStationID, cleanWhitespace(part).c_str(), sizeof(data.completeStationID));

        mem->records[current++] = data;
        mem->totalRecords = current;
    }
}

void handleCSVFiles(const fs::path &directory, MemoryBlock *memory)
{
    std::vector<fs::path> csvPaths;

    for (const auto &entry : fs::recursive_directory_iterator(directory))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".csv")
        {
            csvPaths.push_back(entry.path());
        }
    }

    for (const auto &file : csvPaths)
    {
        readCSV(file.string(), memory);
    }
}

int main()
{
    setlocale(LC_NUMERIC, "C");
    fs::path dataPath = "data";
    size_t neededMemory = getMemorySizeForCSV(dataPath);

    std::cout << "Memory required for CSV data: " << neededMemory << " bytes" << std::endl;

    MemoryBlock *sharedMemory = setupMemory("/aqi_memory", sizeof(MemoryBlock));
    if (!sharedMemory)
    {
        std::cerr << "Memory setup failed.\n";
        return 1;
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    handleCSVFiles(dataPath, sharedMemory);
    auto endTime = std::chrono::high_resolution_clock::now();

    auto processingTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "Processed " << sharedMemory->totalRecords << " records in " << processingTime.count() << " milliseconds\n";
    std::cout << "Size of MemoryBlock: " << sizeof(MemoryBlock) << " bytes\n";

    if (munmap(sharedMemory, sizeof(MemoryBlock)) == -1)
    {
        std::cerr << "Error unmapping memory.\n";
    }

    return 0;
}
