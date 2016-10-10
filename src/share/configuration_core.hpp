#pragma once

#include "constants.hpp"
#include "types.hpp"
#include "json/json.hpp"
#include <fstream>
#include <spdlog/spdlog.h>

// Example:
//
// {
//     "profiles": [
//         {
//             "name": "Default profile",
//             "selected": true,
//             "simple_modifications": {
//                 "caps_lock": "delete_or_backspace",
//                 "escape": "spacebar"
//             },
//             "fn_function_keys": {
//                 "f1":  "vk_consumer_brightness_down",
//                 "f2":  "vk_consumer_brightness_up",
//                 "f3":  "vk_mission_control",
//                 "f4":  "vk_launchpad",
//                 "f5":  "vk_consumer_illumination_down",
//                 "f6":  "vk_consumer_illumination_up",
//                 "f7":  "vk_consumer_previous",
//                 "f8":  "vk_consumer_play",
//                 "f9":  "vk_consumer_next",
//                 "f10": "mute",
//                 "f11": "volume_down",
//                 "f12": "volume_up"
//             }
//         },
//         {
//             "name": "Empty",
//             "selected": false
//         }
//     ]
// }

class configuration_core final {
public:
  configuration_core(const configuration_core&) = delete;

  configuration_core(spdlog::logger& logger, const std::string& file_path) : logger_(logger), file_path_(file_path), loaded_(false) {
    std::ifstream input(file_path_);
    if (input) {
      try {
        json_ = nlohmann::json::parse(input);
        loaded_ = true;
      } catch (std::exception& e) {
        logger_.warn("parse error in {0}: {1}", file_path_, e.what());
      }
    }
  }

  configuration_core(spdlog::logger& logger) : configuration_core(logger, get_file_path()) {
  }

  static std::string get_file_path(void) {
    std::string file_path;
    if (auto p = constants::get_configuration_directory()) {
      file_path = p;
      file_path += "/karabiner-elements.json";
    }
    return file_path;
  }

  bool is_loaded(void) const { return loaded_; }

  // std::vector<from,to>
  std::vector<std::pair<krbn::key_code, krbn::key_code>> get_current_profile_simple_modifications(void) const {
    auto profile = get_current_profile();
    return get_key_code_pair_from_json_object(profile["simple_modifications"]);
  }

  // std::vector<f1,vk_consumer_brightness_down>
  std::vector<std::pair<krbn::key_code, krbn::key_code>> get_current_profile_fn_function_keys(void) const {
    auto profile = get_current_profile();
    if (!profile["fn_function_keys"].is_object()) {
      profile = get_default_profile();
    }
    return get_key_code_pair_from_json_object(profile["fn_function_keys"]);
  }

  // std::vector<from,to>
  std::vector<std::pair<krbn::key_code, krbn::key_code>> get_current_profile_standalone_modifiers(void) const {
    auto profile = get_current_profile();
    return get_key_code_pair_from_json_object(profile["standalone_modifiers"]);
  }

  std::string get_current_profile_json(void) const {
    return get_current_profile().dump();
  }

  // Note:
  // Be careful calling `save` method.
  // If the configuration file is corrupted temporarily (user editing the configuration file in editor),
  // the user data will be lost by the `save` method.
  // Thus, we should call the `save` method only when it is neccessary.

  bool save(void) {
    std::ofstream output(file_path_);
    if (!output) {
      return false;
    }

    output << std::setw(4) << json_ << std::endl;
    return true;
  }

private:
  nlohmann::json get_default_profile(void) const {
    nlohmann::json json;
    json["name"] = "Default profile";
    json["selected"] = true;
    json["simple_modifications"] = nlohmann::json::object();
    json["fn_function_keys"]["f1"] = "vk_consumer_brightness_down";
    json["fn_function_keys"]["f2"] = "vk_consumer_brightness_up";
    json["fn_function_keys"]["f3"] = "vk_mission_control";
    json["fn_function_keys"]["f4"] = "vk_launchpad";
    json["fn_function_keys"]["f5"] = "vk_consumer_illumination_down";
    json["fn_function_keys"]["f6"] = "vk_consumer_illumination_up";
    json["fn_function_keys"]["f7"] = "vk_consumer_previous";
    json["fn_function_keys"]["f8"] = "vk_consumer_play";
    json["fn_function_keys"]["f9"] = "vk_consumer_next";
    json["fn_function_keys"]["f10"] = "mute";
    json["fn_function_keys"]["f11"] = "volume_down";
    json["fn_function_keys"]["f12"] = "volume_up";
    return json;
  }

  nlohmann::json get_current_profile(void) const {
    if (json_.is_object() && json_["profiles"].is_array()) {
      for (const auto& profile : json_["profiles"]) {
        if (profile.is_object() && profile["selected"]) {
          return profile;
        }
      }
    }
    return get_default_profile();
  }

  std::vector<std::pair<krbn::key_code, krbn::key_code>> get_key_code_pair_from_json_object(const nlohmann::json& json) const {
    std::vector<std::pair<krbn::key_code, krbn::key_code>> v;

    auto profile = get_current_profile();
    if (json.is_object()) {
      for (auto it = json.begin(); it != json.end(); ++it) {
        std::string from = it.key();
        std::string to = it.value();

        auto from_key_code = krbn::types::get_key_code(from);
        if (!from_key_code) {
          logger_.warn("unknown key_code:{0} in {1}", from, file_path_);
          continue;
        }
        auto to_key_code = krbn::types::get_key_code(to);
        if (!to_key_code) {
          logger_.warn("unknown key_code:{0} in {1}", to, file_path_);
          continue;
        }

        v.push_back(std::make_pair(*from_key_code, *to_key_code));
      }
    }

    return v;
  }

  spdlog::logger& logger_;
  std::string file_path_;

  bool loaded_;
  nlohmann::json json_;
};
