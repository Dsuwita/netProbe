#pragma once

#include "common.h"
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <optional>
#include <span>

namespace netprobe {

// Command-line argument parser
class ArgParser {
public:
    ArgParser(std::string_view description);
    
    // Add positional argument
    void add_positional(std::string name, std::string help);
    
    // Add optional argument
    void add_option(std::string name, std::string short_name, std::string help, 
                   std::optional<std::string> default_value = std::nullopt);
    
    // Add flag (boolean)
    void add_flag(std::string name, std::string short_name, std::string help);
    
    // Parse arguments
    Result<void> parse(int argc, char** argv);
    Result<void> parse(std::span<const char*> args);
    
    // Get values
    std::optional<std::string> get(std::string_view name) const;
    std::vector<std::string> get_positional() const;
    bool get_flag(std::string_view name) const;
    
    // Get typed values
    template<typename T>
    std::optional<T> get_as(std::string_view name) const {
        auto value = get(name);
        if (!value) return std::nullopt;
        
        std::istringstream iss(*value);
        T result;
        if (!(iss >> result)) return std::nullopt;
        return result;
    }
    
    std::string help() const;

private:
    struct Option {
        std::string name;
        std::string short_name;
        std::string help;
        bool is_flag;
        std::optional<std::string> default_value;
    };
    
    std::string description_;
    std::vector<std::string> positional_names_;
    std::vector<std::string> positional_help_;
    std::vector<Option> options_;
    
    std::map<std::string, std::string> values_;
    std::map<std::string, bool> flags_;
    std::vector<std::string> positional_values_;
};

} // namespace netprobe
