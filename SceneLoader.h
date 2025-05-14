#pragma once
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iostream>

struct Scene {
    // Global Rendering Settings
    int max_ray_depth;
    int samples_per_pixel;
    Vec3 background_color;

    // Scene Content
    std::map<std::string, Material> materials; // Materials stored by ID
    std::vector<Sphere> objects;               // Scene objects (spheres)
    // std::vector<Light> lights; // If you add lights back

    // Camera (Optional: can be part of Scene or handled separately)
    // CameraData camera_settings; // You might define a CameraData struct

    // Default constructor
    Scene() : max_ray_depth(5), samples_per_pixel(1), background_color(0.2, 0.2, 0.2) {}
};

std::vector<std::string> split_string(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}   

Scene load_scene_from_file(const std::string& filename) { // Renamed for clarity
    Scene loaded_scene; // Create a Scene object to populate
    bool success = true; // Flag to track parsing success

    std::ifstream file(filename);
    if (!file.is_open()) {
        OutputDebugStringA(("Error: Could not open scene file: " + filename + "\n").c_str());
        // Return a default (or empty) scene on failure, or throw an exception
        return Scene(); // Returns a default-constructed scene
    }

    std::string line;
    int line_number = 0;
    bool globals_loaded = false;

    while (std::getline(file, line)) {
        line_number++;
        line.erase(0, line.find_first_not_of(" \t\n\r\f\v"));
        line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);

        if (line.empty() || line[0] == '#') continue;

        std::vector<std::string> tokens = split_string(line, ';');
        if (tokens.empty()) continue;

        try {
            if (!globals_loaded) {
                if (tokens.size() < 5) {
                    OutputDebugStringA(("Error: Insufficient global settings on line " + std::to_string(line_number) + "\n").c_str());
                    success = false; break; // Stop parsing on critical error
                }
                loaded_scene.max_ray_depth = std::stoi(tokens[0]);
                loaded_scene.samples_per_pixel = std::stoi(tokens[1]);
                loaded_scene.background_color.x = std::stod(tokens[2]);
                loaded_scene.background_color.y = std::stod(tokens[3]);
                loaded_scene.background_color.z = std::stod(tokens[4]);
                globals_loaded = true;
                continue;
            }

            char type = tokens[0][0];
            if (type == 'M') {
                if (tokens.size() < 7) { /* ... error handling ... */ continue; }
                std::string id = tokens[1];
                Vec3 color(std::stod(tokens[2]), std::stod(tokens[3]), std::stod(tokens[4]));
                double reflectivity = std::stod(tokens[5]);
                double roughness = std::stod(tokens[6]);
                Vec3 emission(0, 0, 0);
                if (tokens.size() >= 10) {
                    emission.x = std::stod(tokens[7]);
                    emission.y = std::stod(tokens[8]);
                    emission.z = std::stod(tokens[9]);
                }
                loaded_scene.materials[id] = Material(id, color, reflectivity, roughness, emission);
            }
            else if (type == 'S') {
                if (tokens.size() < 6) { /* ... error handling ... */ continue; }
                std::string mat_id_ref = tokens[1];
                Vec3 center(std::stod(tokens[2]), std::stod(tokens[3]), std::stod(tokens[4]));
                double radius = std::stod(tokens[5]);

                auto it = loaded_scene.materials.find(mat_id_ref);
                if (it != loaded_scene.materials.end()) {
                    loaded_scene.objects.emplace_back(Sphere(center, radius, it->second /* a copy of the material */));
                }
                else {
                    OutputDebugStringA(("Error: Material ID '" + mat_id_ref + "' not found for sphere on line " + std::to_string(line_number) + "\n").c_str());
                    // Optionally set success = false; or skip this sphere
                }
            }
            else {
                OutputDebugStringA(("Warning: Unknown object type '" + std::string(1, type) + "' on line " + std::to_string(line_number) + "\n").c_str());
            }
        }
        catch (const std::exception& e) { // Catch std::exception for broader coverage
            OutputDebugStringA(("Error parsing line " + std::to_string(line_number) + ": " + e.what() + "\n").c_str());
            success = false; // Potentially stop parsing or just log and continue
        }
    }
    file.close();

    if (!globals_loaded && line_number > 0 && success) { // only an error if parsing was otherwise ok
        OutputDebugStringA("Error: Global settings not found or no valid lines in scene file.\n");
        success = false;
    }

    if (!success) {
        // If critical errors occurred, you might want to return a clearly invalid/default scene
        OutputDebugStringA("Scene loading finished with errors. Returning default scene.\n");
        return Scene(); // Return default scene
    }
    return loaded_scene;
}