#include <stdio.h>
#include <string.h>

#include "SwitchBluetooth.h"
#include "CommandParser.h"
#include "FastLogger.h"
#include "pico/stdlib.h"
#include "btstack.h"

SwitchBluetooth *switchController = nullptr;
CommandParser *commandParser = nullptr;

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_timer_source_t serial_timer;

static void packet_handler_wrapper(uint8_t packet_type, uint16_t channel,
                                   uint8_t *packet, uint16_t packet_size) {
  packet_handler(switchController, packet_type, packet);
}

static void hid_report_data_callback_wrapper(uint16_t cid,
                                             hid_report_type_t report_type,
                                             uint16_t report_id,
                                             int report_size, uint8_t *report) {
  // USB report callback includes 2 bytes excluded here, prepending 2 bytes to
  // keep alignment
  hid_report_data_callback(switchController, report_id, report - 1,
                           report_size + 1);
}

void process_serial_commands() {
    static char line_buffer[128]; // Reduced buffer size
    static int buffer_pos = 0;
    
    // Update non-blocking sleep state
    commandParser->update_sleep_state();

    // Process deferred press releases
    commandParser->update_press_state();

    // Skip command processing if we're in a sleep state
    if (commandParser->is_sleeping()) {
        return;
    }
    
    // Process multiple characters per call to reduce overhead
    for (int i = 0; i < 16; i++) { // Process up to 16 chars at once
        int c = getchar_timeout_us(0); // Non-blocking read
        if (c == PICO_ERROR_TIMEOUT) {
            break; // No more data available
        }
        
        if (c == '\n' || c == '\r') {
            if (buffer_pos > 0) {
                line_buffer[buffer_pos] = '\0';
                
                // Execute command - timing is now handled in SwitchBluetooth
                commandParser->parse_and_execute(line_buffer);
                
                buffer_pos = 0;
            }
        } else if (buffer_pos < sizeof(line_buffer) - 1) {
            line_buffer[buffer_pos++] = c;
        } else {
            // Buffer overflow - reset silently
            buffer_pos = 0;
        }
    }
}

static void serial_timer_handler(btstack_timer_source_t *ts) {
    // Process serial commands with minimal latency
    process_serial_commands();
    
    // Process any remaining queued commands for reliability
    if (switchController && switchController->has_queued_commands()) {
        switchController->process_command_queue();
    }
    
    // Flush logs non-blocking way (only when there's time)
    if (FastLogger::has_pending_logs()) {
        FastLogger::flush_logs();
    }
    
    // Reschedule timer for next check - 1ms for maximum responsiveness
    btstack_run_loop_set_timer(ts, 1);  // 1ms intervals for ultra-fast command processing
    btstack_run_loop_add_timer(ts);
}

int main() {
  // Initialize stdio USB for serial communication
  stdio_init_all();
  
  // Initialize fast logging system
  FastLogger::init();
  
  // Non-blocking initialization - remove delays that interfere with command processing
  // USB will initialize in background while we start Bluetooth stack
  FastLogger::log("Autoshine Pico Firmware Starting...");
  
  // Initialize Switch controller
  switchController = new SwitchBluetooth();
  commandParser = new CommandParser(switchController);
  
  switchController->init();
  
  FastLogger::log("Bluetooth controller initialized");
  
  hci_event_callback_registration.callback = &packet_handler_wrapper;
  hci_add_event_handler(&hci_event_callback_registration);

  hid_device_register_packet_handler(&packet_handler_wrapper);
  hid_device_register_report_data_callback(&hid_report_data_callback_wrapper);

  // Set up periodic timer for serial command processing - ultra-fast 1ms intervals
  serial_timer.process = &serial_timer_handler;
  btstack_run_loop_set_timer(&serial_timer, 1);  // Start after 1ms 
  btstack_run_loop_add_timer(&serial_timer);

  // turn on!
  hci_power_control(HCI_POWER_ON);
  
  FastLogger::log("Bluetooth stack started. Waiting for commands over USB serial...");
  FastLogger::log("Available commands:");
  FastLogger::log("  PRESS <button>      - Press and release a button");
  FastLogger::log("  HOLD <button>       - Hold a button down (without releasing)");
  FastLogger::log("  RELEASE <button>    - Release a button");  
  FastLogger::log("  STICK <stick> <h> <v> - Set stick position (-1.0 to 1.0)");
  FastLogger::log("  SLEEP <seconds>     - Sleep for specified duration");
  FastLogger::log("  # comment           - Comment line (ignored)");
  FastLogger::log("Ready for commands...");

  // Enter BTStack main event loop (this blocks)
  btstack_run_loop_execute();

  return 0;
}