#include <Arduino.h>

struct CommandResult {
  String id;
  bool success;
  String details;
  String executed_at;
};

void command_init();
bool command_process_response_json(const String& respBody, CommandResult& out);
String command_result_to_json(const CommandResult& r);
uint16_t map_register_name_to_addr(const String& reg);