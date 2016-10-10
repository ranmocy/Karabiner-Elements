#pragma once

#include "constants.hpp"
#include "device_grabber.hpp"
#include "event_manipulator.hpp"
#include "local_datagram_server.hpp"
#include "process_monitor.hpp"
#include "session.hpp"
#include "types.hpp"
#include <vector>

class receiver final {
public:
  receiver(const receiver&) = delete;

  receiver(manipulator::event_manipulator& event_manipulator,
           device_grabber& device_grabber) : event_manipulator_(event_manipulator),
                                             device_grabber_(device_grabber),
                                             exit_loop_(false) {
    const size_t buffer_length = 1024 * 1024;
    buffer_.resize(buffer_length);

    const char* path = constants::get_grabber_socket_file_path();
    unlink(path);
    server_ = std::make_unique<local_datagram_server>(path);

    if (auto uid = session::get_current_console_user_id()) {
      chown(path, *uid, 0);
    }
    chmod(path, 0600);

    exit_loop_ = false;
    thread_ = std::thread([this] { this->worker(); });
  }

  ~receiver(void) {
    unlink(constants::get_grabber_socket_file_path());

    exit_loop_ = true;
    if (thread_.joinable()) {
      thread_.join();
    }

    server_ = nullptr;
    console_user_server_process_monitor_ = nullptr;
    device_grabber_.ungrab_devices();
    event_manipulator_.clear_simple_modifications();
    event_manipulator_.clear_fn_function_keys();
    event_manipulator_.clear_standalone_modifiers();
  }

private:
  void worker(void) {
    if (!server_) {
      return;
    }

    while (!exit_loop_) {
      boost::system::error_code ec;
      std::size_t n = server_->receive(boost::asio::buffer(buffer_), boost::posix_time::seconds(1), ec);

      if (!ec && n > 0) {
        switch (krbn::operation_type(buffer_[0])) {
        case krbn::operation_type::connect:
          if (n != sizeof(krbn::operation_type_connect_struct)) {
            logger::get_logger().error("invalid size for krbn::operation_type::connect");
          } else {
            auto p = reinterpret_cast<krbn::operation_type_connect_struct*>(&(buffer_[0]));

            switch (p->connect_from) {
            case krbn::connect_from::event_dispatcher:
              logger::get_logger().info("karabiner_event_dispatcher is connected (pid:{0})", p->pid);
              event_manipulator_.create_event_dispatcher_client();
              break;

            case krbn::connect_from::console_user_server:
              logger::get_logger().info("karabiner_console_user_server is connected (pid:{0})", p->pid);

              device_grabber_.grab_devices();

              // monitor the last process
              console_user_server_process_monitor_ = nullptr;
              console_user_server_process_monitor_ = std::make_unique<process_monitor>(logger::get_logger(),
                                                                                       p->pid,
                                                                                       std::bind(&receiver::console_user_server_exit_callback, this));

              break;
            }
          }
          break;

        case krbn::operation_type::system_preferences_values_updated:
          if (n < sizeof(krbn::operation_type_system_preferences_values_updated_struct)) {
            logger::get_logger().error("invalid size for krbn::operation_type::system_preferences_values_updated ({0})", n);
          } else {
            auto p = reinterpret_cast<krbn::operation_type_system_preferences_values_updated_struct*>(&(buffer_[0]));
            event_manipulator_.set_system_preferences_values(p->values);
            logger::get_logger().info("system_preferences_values_updated");
          }
          break;

        case krbn::operation_type::set_caps_lock_led_state:
          if (n < sizeof(krbn::operation_type_set_caps_lock_led_state_struct)) {
            logger::get_logger().error("invalid size for krbn::operation_type::set_caps_lock_led_state ({0})", n);
          } else {
            auto p = reinterpret_cast<krbn::operation_type_set_caps_lock_led_state_struct*>(&(buffer_[0]));
            // bind variables
            auto led_state = p->led_state;
            device_grabber_.set_caps_lock_led_state(led_state);
          }
          break;

        case krbn::operation_type::clear_simple_modifications:
          event_manipulator_.clear_simple_modifications();
          break;

        case krbn::operation_type::add_simple_modification:
          if (n < sizeof(krbn::operation_type_add_simple_modification_struct)) {
            logger::get_logger().error("invalid size for krbn::operation_type::add_simple_modification ({0})", n);
          } else {
            auto p = reinterpret_cast<krbn::operation_type_add_simple_modification_struct*>(&(buffer_[0]));
            event_manipulator_.add_simple_modification(p->from_key_code, p->to_key_code);
          }
          break;

        case krbn::operation_type::clear_fn_function_keys:
          event_manipulator_.clear_fn_function_keys();
          break;

        case krbn::operation_type::add_fn_function_key:
          if (n < sizeof(krbn::operation_type_add_fn_function_key_struct)) {
            logger::get_logger().error("invalid size for krbn::operation_type::add_fn_function_key ({0})", n);
          } else {
            auto p = reinterpret_cast<krbn::operation_type_add_fn_function_key_struct*>(&(buffer_[0]));
            event_manipulator_.add_fn_function_key(p->from_key_code, p->to_key_code);
          }
          break;

        case krbn::operation_type::clear_standalone_modifiers:
          event_manipulator_.clear_standalone_modifiers();
          break;

        case krbn::operation_type::add_standalone_modifier:
          if (n < sizeof(krbn::operation_type_add_standalone_modifier_struct)) {
            logger::get_logger().error("invalid size for krbn::operation_type::add_standalone_modifier ({0})", n);
          } else {
            auto p = reinterpret_cast<krbn::operation_type_add_standalone_modifier_struct*>(&(buffer_[0]));
            event_manipulator_.add_standalone_modifier(p->from_key_code, p->to_key_code);
          }
          break;

        default:
          break;
        }
      }
    }
  }

  void console_user_server_exit_callback(void) {
    device_grabber_.ungrab_devices();
  }

  manipulator::event_manipulator& event_manipulator_;
  device_grabber& device_grabber_;

  std::vector<uint8_t> buffer_;
  std::unique_ptr<local_datagram_server> server_;
  std::thread thread_;
  std::atomic<bool> exit_loop_;

  std::unique_ptr<process_monitor> console_user_server_process_monitor_;
};
