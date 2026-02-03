#include "CommandParser.h"
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cstdio>
#include "pico/stdlib.h"
#include "FastLogger.h"

CommandParser::CommandParser(SwitchBluetooth* switch_controller) : _switch(switch_controller) {}

bool CommandParser::parse_and_execute(const char* command_line) {
    // Skip leading whitespace
    const char* ptr = command_line;
    skip_whitespace(ptr);
    
    if (*ptr == '\0' || *ptr == '#') {
        return true; // Empty line or comment
    }
    
    // Fast command identification by first character to avoid string comparisons
    char first_char = toupper(*ptr);
    
    // Extract command
    char command[16]; // Reduced buffer size
    int i = 0;
    while (*ptr && !isspace(*ptr) && i < sizeof(command) - 1) {
        command[i++] = toupper(*ptr++);
    }
    command[i] = '\0';
    
    // Skip whitespace between command and args
    skip_whitespace(ptr);
    
    // Execute command using optimized switch
    switch (first_char) {
        case 'P':
            if (command[1] == 'R') { // "PRESS"
                return parse_press_command(ptr);
            }
            break;
        case 'H':
            if (command[1] == 'O') { // "HOLD" 
                return parse_button_command(ptr, true);
            }
            break;
        case 'R':
            if (command[1] == 'E') { // "RELEASE"
                return parse_button_command(ptr, false);
            }
            break;
        case 'S':
            if (command[1] == 'T' && command[2] == 'I') { // "STICK"
                return parse_stick_command(ptr);
            } else if (command[1] == 'T' && command[2] == 'A') { // "STATE"
                return parse_state_command(ptr);
            } else if (command[1] == 'L') { // "SLEEP"
                return parse_sleep_command(ptr);
            }
            break;
        default:
            FastLogger::log_fmt("Unknown command: %s", command);
            return false;
    }
    
    FastLogger::log_fmt("Unknown command: %s", command);
    return false;
}

bool CommandParser::parse_button_command(const char* args, bool pressed) {
    const char* ptr = args;
    char button_name[16]; // Reduced buffer size
    
    // Start consolidation to group all buttons in one HID report
    _switch->start_consolidation();
    
    // Parse multiple button names separated by spaces
    while (*ptr) {
        if (!parse_button_name(ptr, button_name, sizeof(button_name))) {
            break; // No more buttons to parse
        }
        
        _switch->set_button(button_name, pressed);
        
        // Skip to next button
        skip_whitespace(ptr);
    }
    
    // End consolidation and transmit as single frame
    _switch->end_consolidation();
    
    return true;
}

bool CommandParser::parse_press_command(const char* args) {
    // PRESS command: press buttons now, defer release until after the
    // pressed state has been transmitted in an HID report.
    const char* ptr = args;
    char button_name[16];

    // Start consolidation for press phase
    _switch->start_consolidation();

    // Press all buttons in same frame
    while (*ptr) {
        if (!parse_button_name(ptr, button_name, sizeof(button_name))) {
            break;
        }

        _switch->set_button(button_name, true);
        skip_whitespace(ptr);
    }

    // End press consolidation and transmit
    _switch->end_consolidation();

    // Store args for deferred release
    strncpy(_press_release_args, args, sizeof(_press_release_args) - 1);
    _press_release_args[sizeof(_press_release_args) - 1] = '\0';
    _press_release_pending = true;
    _press_release_time = to_ms_since_boot(get_absolute_time());

    return true;
}

bool CommandParser::parse_stick_command(const char* args) {
    const char* ptr = args;
    skip_whitespace(ptr);
    
    // Parse stick name
    char stick_name[32];
    if (!parse_button_name(ptr, stick_name, sizeof(stick_name))) {
        FastLogger::log("Invalid stick name for STICK command");
        return false;
    }
    
    // Parse horizontal value
    float h, v;
    if (!parse_float(ptr, h)) {
        FastLogger::log("Invalid horizontal value for STICK command");
        return false;
    }
    
    // Parse vertical value
    if (!parse_float(ptr, v)) {
        FastLogger::log("Invalid vertical value for STICK command");
        return false;
    }
    
    // Use consolidation for stick commands too
    _switch->start_consolidation();
    _switch->set_stick(stick_name, h, v);
    _switch->end_consolidation();
    
    return true;
}

bool CommandParser::parse_state_command(const char* args) {
    // STATE <18 binary digits> [LH LV RH RV]
    // Button order: A B X Y  L R ZL ZR  + - Home Capture  L3 R3  DUp DDown DLeft DRight
    // Stick values are optional floats in [-1.0, 1.0], default to 0.0
    static const char* button_names[] = {
        "a", "b", "x", "y",
        "l", "r", "zl", "zr",
        "plus", "minus", "home", "capture",
        "l_stick", "r_stick",
        "dpad_up", "dpad_down", "dpad_left", "dpad_right"
    };
    static const int NUM_BUTTONS = 18;

    const char* ptr = args;
    skip_whitespace(ptr);

    // Validate we have enough binary digits
    int len = 0;
    while (ptr[len] == '0' || ptr[len] == '1') len++;
    if (len != NUM_BUTTONS) {
        FastLogger::log_fmt("STATE requires %d binary digits, got %d", NUM_BUTTONS, len);
        return false;
    }

    _switch->start_consolidation();

    for (int i = 0; i < NUM_BUTTONS; i++) {
        _switch->set_button(button_names[i], ptr[i] == '1');
    }
    ptr += NUM_BUTTONS;

    // Parse optional stick values: LH LV RH RV
    float lh = 0.0f, lv = 0.0f, rh = 0.0f, rv = 0.0f;
    if (parse_float(ptr, lh)) {
        if (parse_float(ptr, lv)) {
            _switch->set_stick("l_stick", lh, lv);
            if (parse_float(ptr, rh) && parse_float(ptr, rv)) {
                _switch->set_stick("r_stick", rh, rv);
            }
        }
    }

    _switch->end_consolidation();
    return true;
}

bool CommandParser::parse_sleep_command(const char* args) {
    const char* ptr = args;
    skip_whitespace(ptr);
    
    float duration;
    if (!parse_float(ptr, duration)) {
        // Use fast logger instead of blocking printf
        return false;
    }
    
    // Convert to milliseconds and set up non-blocking sleep
    uint32_t sleep_duration_ms = (uint32_t)(duration * 1000);
    _sleep_active = true;
    _sleep_end_time = to_ms_since_boot(get_absolute_time()) + sleep_duration_ms;
    
    return true;
}

void CommandParser::update_press_state() {
    if (!_press_release_pending) return;

    // Wait at least 100ms to ensure the pressed state is visible
    uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - _press_release_time;
    if (elapsed < 100) return;

    // Apply the deferred release
    _press_release_pending = false;
    parse_button_command(_press_release_args, false);
}

bool CommandParser::is_sleeping() {
    return _sleep_active;
}

void CommandParser::update_sleep_state() {
    if (_sleep_active && to_ms_since_boot(get_absolute_time()) >= _sleep_end_time) {
        _sleep_active = false;
    }
}

void CommandParser::skip_whitespace(const char*& ptr) {
    while (*ptr && isspace(*ptr)) {
        ptr++;
    }
}

bool CommandParser::parse_float(const char*& ptr, float& value) {
    skip_whitespace(ptr);
    
    char* end_ptr;
    value = strtof(ptr, &end_ptr);
    
    if (ptr == end_ptr) {
        return false; // No digits found
    }
    
    ptr = end_ptr;
    return true;
}

bool CommandParser::parse_button_name(const char*& ptr, char* button_name, size_t max_len) {
    skip_whitespace(ptr);
    
    size_t i = 0;
    while (*ptr && !isspace(*ptr) && i < max_len - 1) {
        button_name[i++] = tolower(*ptr++);
    }
    button_name[i] = '\0';
    
    return i > 0;
}