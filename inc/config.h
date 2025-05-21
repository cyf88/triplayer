#pragma once

#include <iostream>
#include <iomanip>
#include <cstdio>
#include <fstream>
#include <windows.h>
#include <string>
#include <sstream> 
#include <map>


class ConfigReader {
public:
    // 获取静态单例实例
    static ConfigReader& Instance(const std::string& filename = "") {
        static ConfigReader instance(filename);  // 仅在第一次调用时加载配置文件
        if (!filename.empty()) {
            instance.loadConfig(filename);  // 允许在之后调用时重新加载配置文件
        }
        return instance;
    }

    // 获取指定节的配置项的值
    std::string getValue(const std::string& section, const std::string& key) const {
        auto sectionIt = configMap.find(section);
        if (sectionIt != configMap.end()) {
            auto keyIt = sectionIt->second.find(key);
            if (keyIt != sectionIt->second.end()) {
                return keyIt->second;
            }
        }
        return ""; // 如果没有找到，返回空字符串
    }

    // 获取某个节下的所有配置项
    void printSection(const std::string& section) const {
        auto sectionIt = configMap.find(section);
        if (sectionIt != configMap.end()) {
            std::cout << "Section: " << section << std::endl;
            for (const auto& [key, value] : sectionIt->second) {
                std::cout << key << " = " << value << std::endl;
            }
        }
        else {
            std::cout << "Section [" << section << "] not found!" << std::endl;
        }
    }

private:
    // 构造函数私有化，确保外部无法直接实例化
    ConfigReader(const std::string& filename = "") {
        if (!filename.empty()) {
            loadConfig(filename);  // 如果文件名不为空，加载配置文件
        }
    }

    // 禁止复制构造函数和赋值操作符
    ConfigReader(const ConfigReader&) = delete;
    ConfigReader& operator=(const ConfigReader&) = delete;

    // 存储配置文件内容，外层是节名，内层是每个节下的键值对
    std::map<std::string, std::map<std::string, std::string>> configMap;

    // 读取配置文件并填充 configMap
    void loadConfig(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open config file: " << filename << std::endl;
            return;
        }

        std::string line;
        std::string currentSection;

        while (std::getline(file, line)) {
            // 跳过空行和注释行（以 '#' 开头）
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // 检查是否为节（如 [SIP_CONFIG]）
            if (line[0] == '[' && line[line.size() - 1] == ']') {
                currentSection = line.substr(1, line.size() - 2);  // 去掉 [ 和 ]
            }
            // 否则处理键值对
            else {
                size_t pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string key = trim(line.substr(0, pos));
                    std::string value = trim(line.substr(pos + 1));
                    configMap[currentSection][key] = value;
                }
            }
        }

        file.close();
    }


    std::string getValueFromConfig(const std::string& configFile, const std::string& section, const std::string& key) {
        std::ifstream configStream(configFile);
        if (!configStream) {
            return "";
        }

        std::string line;
        std::string currentSection;
        bool foundSection = false;

        while (std::getline(configStream, line)) {
            line = trim(line);
            if (line.empty()) {
                continue;
            }

            if (line[0] == '[' && line.back() == ']') {
                currentSection = line.substr(1, line.size() - 2);
                foundSection = (currentSection == section);
            }
            else if (foundSection && std::count(line.begin(), line.end(), '=') == 1) {
                std::istringstream iss(line);
                std::string sectionKey, value;
                if (iss >> sectionKey) {
                    size_t eqPos = line.find('=');
                    value = line.substr(eqPos + 1);
                    sectionKey = line.substr(0, eqPos);
                    sectionKey = trim(sectionKey);
                    value = trim(value);
                    if (sectionKey == key) {
                        return value;
                    }
                }
            }
        }

        return "";
    }


    // 辅助函数：去除字符串前后的空白字符
    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(' ');
        if (std::string::npos == first) {
            return str;
        }
        size_t last = str.find_last_not_of(' ');
        return str.substr(first, (last - first + 1));
    }
};
