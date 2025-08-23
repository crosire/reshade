/*
 * Hardware ID Verification System
 * Simple HWID-based license protection
 */

#pragma once
#include <Windows.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstdlib>

namespace reshade::hwid {

    /// <summary>
    /// Get CPU serial number
    /// </summary>
    inline std::string get_cpu_id() {
        int cpu_info[4];
        __cpuid(cpu_info, 0x00000001);
        
        std::stringstream ss;
        ss << std::hex << cpu_info[3] << cpu_info[0];
        return ss.str();
    }

    /// <summary>
    /// Get volume serial number of C: drive
    /// </summary>
    inline std::string get_volume_serial() {
        DWORD serial_number = 0;
        GetVolumeInformationA("C:\\", nullptr, 0, &serial_number, nullptr, nullptr, nullptr, 0);
        
        std::stringstream ss;
        ss << std::hex << serial_number;
        return ss.str();
    }

    /// <summary>
    /// Generate unique hardware fingerprint
    /// </summary>
    inline std::string generate_hwid() {
        std::string cpu_id = get_cpu_id();
        std::string vol_serial = get_volume_serial();
        
        // Simple combination - in real implementation, should use proper hashing
        return cpu_id + "_" + vol_serial;
    }

    /// <summary>
    /// Check if current hardware is authorized by reading from external file
    /// </summary>
    inline bool is_authorized() {
        std::string current_hwid = generate_hwid();
        
        // Try to read authorized HWIDs from external file
        std::string license_file_path = std::string(getenv("APPDATA") ? getenv("APPDATA") : "C:") + "\\ReShade\\license.txt";
        
        std::ifstream license_file(license_file_path);
        if (license_file.is_open()) {
            std::string line;
            while (std::getline(license_file, line)) {
                // Remove whitespace and comments
                size_t comment_pos = line.find('#');
                if (comment_pos != std::string::npos) {
                    line = line.substr(0, comment_pos);
                }
                
                // Trim whitespace
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);
                
                if (!line.empty() && line == current_hwid) {
                    license_file.close();
                    return true;
                }
            }
            license_file.close();
        }
        
        // Fallback: check embedded HWID for testing
        const char* test_hwids[] = {
            "12345678_abcdef00",  // Test HWID 1
            "87654321_00fedcba",  // Test HWID 2
            nullptr
        };
        
        for (int i = 0; test_hwids[i] != nullptr; ++i) {
            if (current_hwid == test_hwids[i]) {
                return true;
            }
        }
        
        return false;
    }
}