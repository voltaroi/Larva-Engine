#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <vector>
#include <iostream>

namespace fs = std::filesystem;

class AssetManager {
public:
    /**
     * Initialize assets - extracts Assets.zip if it exists and Assets folder doesn't
     * @return true if assets are available, false otherwise
     */
    static bool InitializeAssets() {
        std::string zipPath = "Assets.zip";
        std::string assetsDir = "Assets";
        
        // Check if Assets folder exists
        if (fs::exists(assetsDir) && fs::is_directory(assetsDir)) {
            std::cout << "[AssetManager] Assets folder found" << std::endl;
            return true; // Assets already extracted or present
        }
        
        // Check if Assets.zip exists
        if (!fs::exists(zipPath)) {
            std::cerr << "[AssetManager] ERROR: Assets.zip not found at: " << fs::absolute(zipPath) << std::endl;
            return false; // No assets found
        }
        
        std::cout << "[AssetManager] Found Assets.zip, extracting..." << std::endl;
        
        // Extract Assets.zip
        bool success = ExtractZip(zipPath, ".");
        
        if (success) {
            std::cout << "[AssetManager] Assets extracted successfully" << std::endl;
        } else {
            std::cerr << "[AssetManager] ERROR: Failed to extract Assets.zip" << std::endl;
        }
        
        return success;
    }
    
private:
    /**
     * Extract a ZIP file using PowerShell
     */
    static bool ExtractZip(const std::string& zipPath, const std::string& outputDir) {
        // Use Expand-Archive with -Force to overwrite existing files
        std::string command = "PowerShell -NoProfile -Command \"try { "
                            "Expand-Archive -Path '" + zipPath + "' -DestinationPath '" + outputDir + "' -Force; "
                            "exit 0 } "
                            "catch { Write-Host ('ERROR: ' + $_.Exception.Message); exit 1 }\"";
        
        std::cout << "[AssetManager] Executing extraction command..." << std::endl;
        int result = system(command.c_str());
        
        return result == 0;
    }
};