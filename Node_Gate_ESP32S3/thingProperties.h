/*
 * =====================================================
 * thingProperties.h
 * FIXED: Added setBoardId() to fix Error 5
 * =====================================================
 */
#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>

// ⚠️ Credentials ของคุณ
const char WIFI_SSID[]     = "AB";
const char WIFI_PASSWORD[] = "12345678";

const char DEVICE_LOGIN_NAME[] = "f9361716-d489-479f-b46a-9a0ba706d094";
const char DEVICE_KEY[]    = "5kUe@s3kPhm3BdraQb18UxWsT";

// ตัวแปร Cloud
String boat_status;
String command;
float current_x;
float current_y;
float distance_a1;
float distance_a2;
float distance_a3;
bool emergency_stop;
float target_x;
float target_y;

void onCommandChange();
void onEmergencyStopChange();
void onTargetXChange();
void onTargetYChange();
void onCurrentXChange();
void onCurrentYChange();
void onDistanceA1Change();
void onDistanceA2Change();
void onDistanceA3Change();

void initProperties() {
  // ✅ ต้องใส่บรรทัดนี้ด้วย ไม่งั้นจะ Error 5
  ArduinoCloud.setBoardId(DEVICE_LOGIN_NAME);
  ArduinoCloud.setSecretDeviceKey(DEVICE_KEY);
  
  ArduinoCloud.addProperty(boat_status, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(command, READWRITE, ON_CHANGE, onCommandChange);
  
  ArduinoCloud.addProperty(current_x, READWRITE, ON_CHANGE, onCurrentXChange);
  ArduinoCloud.addProperty(current_y, READWRITE, ON_CHANGE, onCurrentYChange);
  
  ArduinoCloud.addProperty(distance_a1, READWRITE, ON_CHANGE, onDistanceA1Change);
  ArduinoCloud.addProperty(distance_a2, READWRITE, ON_CHANGE, onDistanceA2Change);
  ArduinoCloud.addProperty(distance_a3, READWRITE, ON_CHANGE, onDistanceA3Change);
  
  ArduinoCloud.addProperty(emergency_stop, READWRITE, ON_CHANGE, onEmergencyStopChange);
  ArduinoCloud.addProperty(target_x, READWRITE, ON_CHANGE, onTargetXChange);
  ArduinoCloud.addProperty(target_y, READWRITE, ON_CHANGE, onTargetYChange);
}

WiFiConnectionHandler ArduinoIoTPreferredConnection(WIFI_SSID, WIFI_PASSWORD);
