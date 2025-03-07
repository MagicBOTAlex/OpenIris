#include "SerialManager.hpp"
#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "boostiest booster";
const char* pwd = "crystalize";

const char* udpAddress = "192.168.50.58";
const int udpPort = 3333;

WiFiUDP udp;

// Image is quite large, so many frags
#define UDP_FRAG_NUM 12
#define UDP_RETRY 5
#define UDP_PACKET_SIZE (size_t)1024

// UDP has max packet size
struct PacketHeader {
  int frameNum;
  int id;
  int size;
  int totalPackets;
} __attribute__((packed));

int currentFrameNum = 0;
bool confirmed[UDP_FRAG_NUM];

void createByteArray(uint8_t** arr, size_t* len) {
  *len = 255; // 0x01 to 0xFF (255 values)
  *arr = (uint8_t*)malloc(*len); // Allocate memory for the array

  if (*arr == NULL) {
      *len = 0; // If malloc fails, set length to 0
      return;
  }

  for (size_t i = 0; i < *len; i++) {
      (*arr)[i] = i + 1; // Fill array from 0x01 to 0xFF
  }
}

void sendPacket(PacketHeader* packetHeader, uint8_t* data){
  udp.beginPacket(udpAddress, udpPort);
  udp.write((uint8_t *)packetHeader, sizeof(PacketHeader));
  udp.write((uint8_t *)data, packetHeader->size);
  udp.endPacket();
}

SerialManager::SerialManager(CommandManager* commandManager)
    : commandManager(commandManager) {}

#ifdef ETVR_EYE_TRACKER_USB_API
void SerialManager::send_frame() {
  if (!last_frame)
    last_frame = esp_timer_get_time();

  uint8_t len_bytes[2];

  size_t len = 0;
  uint8_t* buf = NULL;

  // createByteArray(&buf, &len);

  auto fb = esp_camera_fb_get();
  if (fb) {
    len = fb->len;
    buf = fb->buf;
  } else
    err = ESP_FAIL;

  // if we failed to capture the frame, we bail, but we still want to listen to
  // commands
  if (err != ESP_OK) {
    log_e("Camera capture failed with response: %s", esp_err_to_name(err));
    return;
  }

  udp.flush();
  currentFrameNum++;

  memset(confirmed, false, sizeof(confirmed)); // Clear confirmed
  PacketHeader packetHeader;
  for (size_t sendInteration = 0; sendInteration < 1 ; sendInteration++)
  {
    for (size_t i = 0; i < UDP_FRAG_NUM; i++)
    {
      if (sendInteration > 0 && i == 0) continue; // Skip first packet if resending the rest

      packetHeader.frameNum = currentFrameNum;
      packetHeader.id = i;
      packetHeader.totalPackets = ceil(len / UDP_PACKET_SIZE ) +1;

      size_t offset = min(i * UDP_PACKET_SIZE, len);
      packetHeader.size = min(UDP_PACKET_SIZE, len - offset);

      // Serial.print("Sending size of: ");
      // Serial.println(sizeof(int) * 2 + );

      sendPacket(&packetHeader, buf + offset);
      // vTaskDelay(1); 
    }
  }
  

  // bool confirmedMissing = true; // Assume missing
  // for (size_t i = 0; i < UDP_RETRY; i++)
  // {
  //   // Check if all ACK
  //   for (size_t p = 0; p < UDP_FRAG_NUM; p++)
  //   {
  //     confirmedMissing &= confirmed[p];
  //   }
  //   if (!confirmedMissing) break;
    

  //   vTaskDelay(pdMS_TO_TICKS(50));
    
  //   while (udp.available()) {
  //     String response = udp.readString();
  //     if (response == "ERR") {
  //       vTaskDelay(pdMS_TO_TICKS(100));
  //       return; // Restart from back
  //     }
  //     int colonIndex = response.indexOf(':');
  //     int colonIndex2 = response.indexOf(':', colonIndex);
  //     if (colonIndex != -1) {
  //       int packetId = response.substring(0, colonIndex).toInt();
  //       int packetFrameNum = response.substring(colonIndex, colonIndex2).toInt();
  //       if (packetFrameNum == currentFrameNum){
  //         confirmed[packetId] = true;

  //       }
  //     }
  //   }

  //   // Retry send
  //   for (size_t i = 0; i < UDP_FRAG_NUM; i++)
  //   {
  //     if (confirmed[i]) continue;
      
  //     // Serial.print("Packet ");
  //     // Serial.print(i);
  //     // Serial.println(" dropped. Retrying");
  //     packetHeader.frameNum = currentFrameNum;
  //     packetHeader.id = i;
  //     packetHeader.totalPackets = ceil(len / UDP_PACKET_SIZE) + 1;

  //     size_t offset = min(i * UDP_PACKET_SIZE, len);
  //     packetHeader.size = min(UDP_PACKET_SIZE, len - offset);

  //     sendPacket(&packetHeader, buf + offset);
  //   }
  // }
  
  

  if (fb) {
    esp_camera_fb_return(fb);
    fb = NULL;
    buf = NULL;
  } else if (buf) {
    free(buf);
    buf = NULL;
  }

  vTaskDelay(pdMS_TO_TICKS(1000/90));

  free(buf);

  long request_end = millis();
  long latency = request_end - last_request_time;
  last_request_time = request_end;
  Serial.printf("Size: %uKB, Time: %ums (%ifps)\n", len / 1024, latency,
        1000 / latency);
}
#endif

void SerialManager::init() {
  #ifdef SERIAL_MANAGER_USE_HIGHER_FREQUENCY
      Serial.begin(3000000);
  #endif
      if (SERIAL_FLUSH_ENABLED) {
          Serial.flush();
      }
  
      Serial.println("Starting WiFi connection...");
      WiFi.begin(ssid, pwd);
  
      // Wait until WiFi is connected
      int attempt = 0;
      while (WiFi.status() != WL_CONNECTED && attempt < 20) {
          Serial.print(".");
          vTaskDelay(pdMS_TO_TICKS(500));
          attempt++;
      }
  
      if (WiFi.status() == WL_CONNECTED) {
          Serial.println("\nWiFi connected.");
      } else {
          Serial.println("\nWiFi connection failed.");
          return;
      }
  
      Serial.println("Setting hostname to 'Balls_sender'...");
      WiFi.setHostname("Balls_sender");
  
      Serial.println("Initializing UDP on port " + String(udpPort) + "...");
      if (udp.begin(udpPort)) {
          Serial.println("UDP initialized successfully.");
      } else {
          Serial.println("UDP initialization failed.");
      }
  }
  

void SerialManager::run() {
  if (Serial.available()) {
    JsonDocument doc;
    DeserializationError deserializationError = deserializeJson(doc, Serial);

    if (deserializationError) {
      log_e("Command deserialization failed: %s", deserializationError.c_str());

      return;
    }

    CommandsPayload commands = {doc};
    this->commandManager->handleCommands(commands);
  }
#ifdef ETVR_EYE_TRACKER_USB_API
  else {
    this->send_frame();
  }
#endif
}
