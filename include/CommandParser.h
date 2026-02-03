#ifndef CommandParser_h
#define CommandParser_h

#include "SwitchBluetooth.h"

class CommandParser {
public:
    CommandParser(SwitchBluetooth* switch_controller);
    
    // Parse and execute a command line
    bool parse_and_execute(const char* command_line);
    
    // Check if currently in a sleep state (non-blocking)
    bool is_sleeping();
    void update_sleep_state();

    // Deferred press-release handling
    void update_press_state();
    
private:
    SwitchBluetooth* _switch;
    
    // Non-blocking sleep state
    bool _sleep_active = false;
    uint32_t _sleep_end_time = 0;

    // Deferred release for PRESS command
    bool _press_release_pending = false;
    uint32_t _press_release_time = 0;
    char _press_release_args[128];
    
    // Command parsing helpers
    bool parse_button_command(const char* args, bool pressed);
    bool parse_press_command(const char* args);  // Press and release with timing
    bool parse_stick_command(const char* args);
    bool parse_state_command(const char* args);
    bool parse_sleep_command(const char* args);
    
    // Utility functions
    void skip_whitespace(const char*& ptr);
    bool parse_float(const char*& ptr, float& value);
    bool parse_button_name(const char*& ptr, char* button_name, size_t max_len);
};

#endif