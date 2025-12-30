#include "argparse.h"
#include <sstream>
#include <format>
#include <charconv>

namespace netprobe {

ArgParser::ArgParser(std::string_view description) 
    : description_(description) {}

void ArgParser::add_positional(std::string name, std::string help) {
    positional_names_.push_back(std::move(name));
    positional_help_.push_back(std::move(help));
}

void ArgParser::add_option(std::string name, std::string short_name, 
                          std::string help, std::optional<std::string> default_value) {
    options_.push_back({
        std::move(name),
        std::move(short_name),
        std::move(help),
        false,
        std::move(default_value)
    });
}

void ArgParser::add_flag(std::string name, std::string short_name, std::string help) {
    options_.push_back({
        std::move(name),
        std::move(short_name),
        std::move(help),
        true,
        std::nullopt
    });
}

Result<void> ArgParser::parse(int argc, char** argv) {
    std::vector<const char*> args;
    for (int i = 1; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    return parse(args);
}

Result<void> ArgParser::parse(std::span<const char*> args) {
    // Initialize defaults
    for (const auto& opt : options_) {
        if (opt.default_value) {
            values_[opt.name] = *opt.default_value;
        }
        if (opt.is_flag) {
            flags_[opt.name] = false;
        }
    }
    
    size_t pos_idx = 0;
    
    for (size_t i = 0; i < args.size(); ++i) {
        std::string arg = args[i];
        
        // Check for help
        if (arg == "-h" || arg == "--help") {
            return Result<void>(help());
        }
        
        // Check if it's an option
        if (arg.starts_with("--")) {
            std::string opt_name = arg.substr(2);
            auto it = std::ranges::find_if(options_, 
                [&](const auto& o) { return o.name == opt_name; });
            
            if (it == options_.end()) {
                return Result<void>(std::format("Unknown option: {}", arg));
            }
            
            if (it->is_flag) {
                flags_[it->name] = true;
            } else {
                if (i + 1 >= args.size()) {
                    return Result<void>(std::format("Option {} requires a value", arg));
                }
                values_[it->name] = args[++i];
            }
        } else if (arg.starts_with("-") && arg.length() > 1) {
            std::string short_name = arg.substr(1);
            auto it = std::ranges::find_if(options_,
                [&](const auto& o) { return o.short_name == short_name; });
            
            if (it == options_.end()) {
                return Result<void>(std::format("Unknown option: {}", arg));
            }
            
            if (it->is_flag) {
                flags_[it->name] = true;
            } else {
                if (i + 1 >= args.size()) {
                    return Result<void>(std::format("Option {} requires a value", arg));
                }
                values_[it->name] = args[++i];
            }
        } else {
            // Positional argument
            positional_values_.push_back(arg);
            ++pos_idx;
        }
    }
    
    // Check required positional arguments
    if (positional_values_.size() < positional_names_.size()) {
        return Result<void>(std::format("Missing required argument: {}", 
            positional_names_[positional_values_.size()]));
    }
    
    return Result<void>();
}

std::optional<std::string> ArgParser::get(std::string_view name) const {
    auto it = values_.find(std::string(name));
    if (it != values_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> ArgParser::get_positional() const {
    return positional_values_;
}

bool ArgParser::get_flag(std::string_view name) const {
    auto it = flags_.find(std::string(name));
    return it != flags_.end() && it->second;
}

template<>
std::optional<int> ArgParser::get_as<int>(std::string_view name) const {
    auto val = get(name);
    if (!val) return std::nullopt;
    
    int result;
    auto [ptr, ec] = std::from_chars(val->data(), val->data() + val->size(), result);
    if (ec == std::errc()) return result;
    return std::nullopt;
}

template<>
std::optional<size_t> ArgParser::get_as<size_t>(std::string_view name) const {
    auto val = get(name);
    if (!val) return std::nullopt;
    
    size_t result;
    auto [ptr, ec] = std::from_chars(val->data(), val->data() + val->size(), result);
    if (ec == std::errc()) return result;
    return std::nullopt;
}

template<>
std::optional<double> ArgParser::get_as<double>(std::string_view name) const {
    auto val = get(name);
    if (!val) return std::nullopt;
    
    try {
        return std::stod(*val);
    } catch (...) {
        return std::nullopt;
    }
}

std::string ArgParser::help() const {
    std::ostringstream oss;
    
    oss << description_ << "\n\n";
    
    oss << "Usage: netprobe [options]";
    for (const auto& pos : positional_names_) {
        oss << " <" << pos << ">";
    }
    oss << "\n\n";
    
    if (!positional_names_.empty()) {
        oss << "Arguments:\n";
        for (size_t i = 0; i < positional_names_.size(); ++i) {
            oss << std::format("  {:<20} {}\n", positional_names_[i], positional_help_[i]);
        }
        oss << "\n";
    }
    
    if (!options_.empty()) {
        oss << "Options:\n";
        for (const auto& opt : options_) {
            std::string opt_str = "  ";
            if (!opt.short_name.empty()) {
                opt_str += "-" + opt.short_name + ", ";
            }
            opt_str += "--" + opt.name;
            if (!opt.is_flag) {
                opt_str += " <value>";
            }
            oss << std::format("{:<30} {}", opt_str, opt.help);
            if (opt.default_value) {
                oss << " (default: " << *opt.default_value << ")";
            }
            oss << "\n";
        }
    }
    
    return oss.str();
}

} // namespace netprobe
