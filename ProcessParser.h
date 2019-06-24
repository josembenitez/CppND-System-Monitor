
#pragma once

#include <algorithm>
#include <iostream>
#include <math.h>
#include <thread>
#include <chrono>
#include <iterator>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include "constants.h"
#include "util.h"


using namespace std;

class ProcessParser{
private:
    std::ifstream stream;
    static vector<string> getValues(const string &path, const string &key = "", const string &delim = ":");
public:
    static string getCmd(string pid);
    static vector<string> getPidList();
    static std::string getVmSize(string pid);
    static std::string getCpuPercent(string pid);
    static long int getSysUpTime();
    static std::string getProcUpTime(string pid);
    static string getProcUser(string pid);
    static vector<string> getSysCpuPercent(string coreNumber = "");
    static float getSysRamPercent();
    static string getSysKernelVersion();
    static int getNumberOfCores();
    static int getTotalThreads();
    static int getTotalNumberOfProcesses();
    static int getNumberOfRunningProcesses();
    static string getOSName();
    static std::string PrintCpuStats(std::vector<std::string> values1, std::vector<std::string>values2);
    static bool isPidExisting(string pid);
};

vector<string> ProcessParser::getValues(const string &path, const string &key, const string &delim)
{
    // This method assumes values are separated by whitespaces
    vector<string> values;
    string line;
    ifstream stream;
    Util::getStream(path, stream);
    if (!key.empty())
    {
        while (getline(stream, line))
        {
            const size_t keyPos = line.find(key);
            if (keyPos != string::npos)
            {
                if (!delim.empty())
                {
                    const size_t delimiterPos = line.find(delim);
                    line = line.substr(delimiterPos + 1, line.size());
                }
                else
                {
                    line = line.substr(keyPos + key.size() + 1, line.size());
                }
                
                istringstream iss(line);
                vector<string> items(istream_iterator<string>{iss}, istream_iterator<string>());
                values = vector<string>(items.begin(), items.end());
                break;
            }
        }
    }
    else
    {
        // Return all values in case there's no key to search for
        getline(stream, line);
        istringstream iss(line);
        values = vector<string>(istream_iterator<string>{iss}, istream_iterator<string>());
    }
    return values;
}

// TODO: Define all of the above functions below:
string ProcessParser::getVmSize(string pid)
{
    return getValues(Path::basePath() + pid + Path::statusPath(), "VmData")[0];
}

string ProcessParser::getCpuPercent(string pid)
{
    string data;
    ifstream stream;
    Util::getStream(Path::basePath() + pid + "/" + Path::statPath(), stream);
    getline(stream, data);
    istringstream iss(data);
    vector<string> items(istream_iterator<string>{iss}, istream_iterator<string>());
    const float utime = stof(ProcessParser::getProcUpTime(pid));
    const float stime = stof(items[14]);
    const float cutime = stof(items[15]);
    const float cstime = stof(items[16]);
    const float starttime = stof(items[21]);
    const float uptime = ProcessParser::getSysUpTime();
    const float freq = sysconf(_SC_CLK_TCK);
    const float total_time = utime + stime + cutime + cstime;
    const float seconds = uptime - starttime / freq;
    const float result = 100.0f * ((total_time / freq) / seconds);
    return to_string(result);
}

string ProcessParser::getProcUpTime(string pid)
{
    return to_string(stof(getValues(Path::basePath() + pid + "/" + Path::statPath())[13]) / sysconf(_SC_CLK_TCK));
}

long ProcessParser::getSysUpTime()
{
    return stoi(getValues(Path::basePath() + Path::upTimePath())[0]);
}

string ProcessParser::getProcUser(string pid)
{
    const string uid = getValues(Path::basePath() + pid + Path::statusPath(), "Uid")[0];
    string user;
    string line;
    ifstream stream;
    Util::getStream("/etc/passwd", stream);
    while (getline(stream, line))
    {
        const size_t pos = line.find(":x:" + uid);
        if (pos != string::npos)
        {
            user = line.substr(0, pos);
            break;
        }
    }
    return user;
}

vector<string> ProcessParser::getPidList()
{
    vector<string> pids;

    DIR * const dir = opendir("/proc");
    if (!dir)
    {
        throw runtime_error(strerror(errno));
    }

    while (dirent * const dirp = readdir(dir))
    {
        if (dirp->d_type != DT_DIR)
        {
            continue;
        }

        if (all_of(dirp->d_name, dirp->d_name + strlen(dirp->d_name), [](char c){ return isdigit(c); }))
        {
            pids.push_back(dirp->d_name);
        }
    }

    if (closedir(dir))
    {
        throw runtime_error(strerror(errno));
    }

    return pids;
}

string ProcessParser::getCmd(string pid)
{
    return getValues(Path::basePath() + pid + Path::cmdPath())[0];
}

int ProcessParser::getNumberOfCores()
{
    return stoi(getValues(Path::basePath() + "cpuinfo", "cpu cores")[0]);
}

vector<string> ProcessParser::getSysCpuPercent(string coreNumber)
{
    return getValues(Path::basePath() + Path::statPath(), "cpu" + coreNumber, "");
}

static float getSysActiveCpuTime(const vector<string> &values)
{
    return stof(values[S_USER]) +
           stof(values[S_NICE]) +
           stof(values[S_SYSTEM]) +
           stof(values[S_IRQ]) +
           stof(values[S_SOFTIRQ]) +
           stof(values[S_STEAL]) +
           stof(values[S_GUEST]) +
           stof(values[S_GUEST_NICE]);
}

static float getSysIdleCpuTime(const vector<string> &values)
{
    return stof(values[S_IDLE]) + stof(values[S_IOWAIT]);
}

std::string ProcessParser::PrintCpuStats(std::vector<std::string> values1, std::vector<std::string>values2)
{
    const float activeTime = getSysActiveCpuTime(values2) - getSysActiveCpuTime(values1);
    const float idleTime = getSysIdleCpuTime(values2) - getSysIdleCpuTime(values1);
    const float totalTime = activeTime + idleTime;
    return to_string(100.0f * (activeTime / totalTime));
}

float ProcessParser::getSysRamPercent()
{
    float memAvailable = 0.0f;
    float memFree = 0.0f;
    float buffers = 0.0f;
    size_t count = 0;
    string line;
    ifstream stream;
    
    Util::getStream(Path::basePath() + Path::memInfoPath(), stream);
    while (getline(stream, line))
    {
        if (line.find("MemAvailable") != string::npos)
        {
            istringstream iss(line);
            vector<string> items(istream_iterator<string>{iss}, istream_iterator<string>());
            memAvailable = stof(items[1]);
            ++count;
        }
        else if (line.find("MemFree") != string::npos)
        {
            istringstream iss(line);
            vector<string> items(istream_iterator<string>{iss}, istream_iterator<string>());
            memFree = stof(items[1]);
            ++count;
        }
        else if (line.find("Buffers") != string::npos)
        {
            istringstream iss(line);
            vector<string> items(istream_iterator<string>{iss}, istream_iterator<string>());
            buffers = stof(items[1]);
            ++count;
        }

        if (count == 3)
        {
            break;
        }
    }
    
    return 100.0f * (1 - (memFree / (memAvailable - buffers)));
}

string ProcessParser::getSysKernelVersion()
{
    return getValues(Path::basePath() + Path::versionPath())[2];
}

string ProcessParser::getOSName()
{
    const string key = "PRETTY_NAME";
    string name;
    string line;
    ifstream stream;
    Util::getStream("/etc/os-release", stream);
    while (getline(stream, line))
    {
        if (line.find(key) != string::npos)
        {
            name = line.substr(line.find("=") + 1, line.size());
            break;
        }
    }
    name.erase(remove(name.begin(), name.end(), '"'), name.end());
    return name;
}

int ProcessParser::getTotalThreads()
{
    size_t threads = 0;
    const vector<string> pids = getPidList();
    for (const auto &pid : pids)
    {
        threads += stoi(getValues(Path::basePath() + pid + Path::statusPath(), "Threads")[0]);
    }
    return threads;
}

int ProcessParser::getTotalNumberOfProcesses()
{
    return stoi(getValues(Path::basePath() + Path::statPath(), "processes", "")[0]);
}

int ProcessParser::getNumberOfRunningProcesses()
{
    return stoi(getValues(Path::basePath() + Path::statPath(), "procs_running", "")[0]);
}

bool ProcessParser::isPidExisting(string pid)
{
    const vector<string> pids = getPidList();
    auto found = find(pids.begin(), pids.end(), pid);
    return found != pids.end();
}
