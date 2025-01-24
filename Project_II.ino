#include <SPI.h>
#include <FS.h>
#include <TFT_eSPI.h>
#include <JPEGDecoder.h>
#include <string.h>
TFT_eSPI tft = TFT_eSPI();
#include <WiFi.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <WiFiMulti.h>

#include <HTTPClient.h>

const char WIFI_SSID[] = "Foxie";        // Change this to your WiFi SSID
const char WIFI_PASSWORD[] = "Kenduong";  // Change this to your WiFi password


const char MQTT_BROKER[] = "broker.emqx.io";  // CHANGE TO MQTT BROKER'S ADDRESS
const int MQTT_PORT = 1883;
const char CLIENT_ID[] = "LED_LCD";  // CHANGE IT AS YOU DESIRE
const char MQTT_USERNAME[] = "";     // CHANGE IT IF REQUIRED, empty if not required
const char MQTT_PASSWORD[] = "";     // CHANGE IT IF REQUIRED, empty if not required

// The MQTT topics that ESP32 should publish/subscribe
const char PUBLISH_TOPIC[] = "lcd_test/TOPICPUB";    // CHANGE IT AS YOU DESIRE
const char SUBSCRIBE_TOPIC[] = "lcd_test/TOPICSUB";  // CHANGE IT AS YOU DESIRE

const int PUBLISH_INTERVAL = 5000;  // 5 seconds
// Đây là tên file dùng để lưu trữ dữ liệu hiệu chuẩn
#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false  // true nếu muốn mỗi lần reset phải hiệu chỉnh

WiFiClient network;
MQTTClient mqtt = MQTTClient(256);

unsigned long lastPublishTime = 0;

#define MAX_FILES 20  // Số file tối đa lưu trong danh sách

String fileNames[MAX_FILES];  // Mảng lưu danh sách tên file ảnh
int fileCount = 0;            // Số lượng file ảnh tìm thấy
int currentFileIndex = 0;     // Vị trí file hiện tại trong danh sách

uint16_t t_x = 0, t_y = 0;
uint16_t last_t_x = 0, last_t_y = 0;

void downloadImage(const char *url, const char *filePath);
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  // Khởi tạo màn hình TFT
  tft.begin();
  tft.setRotation(1);
  // touch_calibrate();
  // tft.fillScreen(TFT_BLACK);  // Xóa màn hình

  // // Bật các chân CS của TFT, Touch, và SD Card
  // pinMode(22, OUTPUT);
  // pinMode(15, OUTPUT);
  // pinMode(5, OUTPUT);

  digitalWrite(22, HIGH);  // Touch controller chip select (if used)
  digitalWrite(15, HIGH);  // TFT screen chip select
  digitalWrite(5, HIGH);   // SD card chips select, must use GPIO 5 (ESP32 SS)

  tft.begin();
  if (!SD.begin(5, tft.getSPIinstance())) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  File root = SD.open("/");
  listImages(root);
  Serial.print("Found ");
  Serial.print(fileCount);
  Serial.println(" image files.");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // Wait for connection with timeout (10 seconds)
  unsigned long startMillis = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMillis < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    connectToMQTT();
  } else {
    Serial.println("WiFi connection failed, running without network.");
  }
}
#define SWIPE_THRESHOLD 60  // Ngưỡng vuốt tối thiểu (px)
#define SWIPE_DELAY 500     // Thời gian tối thiểu giữa các lần vuốt (ms)

unsigned long lastSwipeTime = 0;  // Lưu thời gian lần vuốt trước
bool startTouch = false;
uint16_t startX = 0;
void loop() {

  tft.setRotation(1);
  // Hiển thị ảnh hiện tại (chỉ vẽ lại khi cần)
  static int lastFileIndex = -1;
  if (fileCount > 0 && currentFileIndex != lastFileIndex) {
    tft.fillScreen(TFT_BLACK);
    Serial.print("Displaying: ");
    Serial.println(fileNames[currentFileIndex]);
    drawSdJpeg(("/" + fileNames[currentFileIndex]).c_str(), 0, 0);
    lastFileIndex = currentFileIndex;
  }
  if (WiFi.status() == WL_CONNECTED) {
    mqtt.loop();
    if (millis() - lastPublishTime > PUBLISH_INTERVAL) {
      sendToMQTT(fileNames[currentFileIndex].c_str());
      lastPublishTime = millis();
    }
  }
  bool pressed = tft.getTouch(&t_x, &t_y);
  if (pressed) {
    if (startTouch == false) {
      startTouch = true;
      startX = t_x;  // Lưu tọa độ ban đầu khi chạm
    }

    if (millis() - lastSwipeTime > 300) {  // Chống nhiễu thao tác vuốt
      int swipeDistance = t_x - startX;

      if (abs(swipeDistance) > 50) {  // Chỉ xử lý khi vuốt đủ xa
        if (swipeDistance > 0) {
          Serial.println("Vuốt phải -> Next Image");
          currentFileIndex = (currentFileIndex + 1) % fileCount;
        } else {
          Serial.println("Vuốt trái -> Previous Image");
          currentFileIndex = (currentFileIndex - 1 + fileCount) % fileCount;
        }
        lastSwipeTime = millis();  // Cập nhật thời gian vuốt
      }
    }
  } else {
    startTouch = false;  // Reset khi thả tay
  }
}

void listImages(File dir) {
  fileCount = 0;  // Reset số lượng file tìm thấy
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;  // Hết file

    String fileName = entry.name();
    if (fileName.endsWith(".jpg") || fileName.endsWith(".JPG")) {
      if (fileCount < MAX_FILES) {
        fileNames[fileCount] = fileName;
        fileCount++;
      }
    }
    entry.close();
  }
}
void drawSdJpeg(const char *filename, int xpos, int ypos) {
  File jpegFile = SD.open(filename, FILE_READ);
  if (!jpegFile) {
    Serial.print("ERROR: File \"");
    Serial.print(filename);
    Serial.println("\" not found!");
    return;
  }
  Serial.println("===========================");
  Serial.print("Drawing file: ");
  Serial.println(filename);
  Serial.println("===========================");

  bool decoded = JpegDec.decodeSdFile(jpegFile);
  if (decoded) {
    // render the image onto the screen at given coordinates
    jpegRender(xpos, ypos);
  } else {
    Serial.println("Jpeg file format not supported!");
  }
}
void jpegRender(int xpos, int ypos) {
  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  bool swapBytes = tft.getSwapBytes();
  tft.setSwapBytes(true);

  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = jpg_min(mcu_w, max_x % mcu_w);
  uint32_t min_h = jpg_min(mcu_h, max_y % mcu_h);

  // save the current image block size
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  // record the current time so we can measure how long it takes to draw an image
  uint32_t drawTime = millis();

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // Fetch data from the file, decode and display
  while (JpegDec.read()) {  // While there is more data in the file
    pImg = JpegDec.pImage;  // Decode a MCU (Minimum Coding Unit, typically a 8x8 or 16x16 pixel block)

    // Calculate coordinates of top left corner of current MCU
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    // check if the image block size needs to be changed for the right edge
    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;

    // check if the image block size needs to be changed for the bottom edge
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

    // copy pixels into a contiguous block
    if (win_w != mcu_w) {
      uint16_t *cImg;
      int p = 0;
      cImg = pImg + win_w;
      for (int h = 1; h < win_h; h++) {
        p += mcu_w;
        for (int w = 0; w < win_w; w++) {
          *cImg = *(pImg + w + p);
          cImg++;
        }
      }
    }

    // calculate how many pixels must be drawn
    uint32_t mcu_pixels = win_w * win_h;

    // draw image MCU block only if it will fit on the screen
    if ((mcu_x + win_w) <= tft.width() && (mcu_y + win_h) <= tft.height())
      tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
    else if ((mcu_y + win_h) >= tft.height())
      JpegDec.abort();  // Image has run off bottom of screen so abort decoding
  }

  tft.setSwapBytes(swapBytes);

  showTime(millis() - drawTime);  // These lines are for sketch testing only
}
void showTime(uint32_t msTime) {
  tft.setCursor(0, 0);
  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print(F(" JPEG drawn in "));
  tft.print(msTime);
  tft.println(F(" ms "));
  Serial.print(F(" JPEG drawn in "));
  Serial.print(msTime);
  Serial.println(F(" ms "));
}
void connectToMQTT() {
  mqtt.begin(MQTT_BROKER, MQTT_PORT, network);

  mqtt.onMessage(messageHandler);

  Serial.print("ESP32 - Connecting to MQTT broker");

  while (!mqtt.connect(CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (!mqtt.connected()) {
    Serial.println("ESP32 - MQTT broker Timeout!");
    return;
  }
  if (mqtt.subscribe(SUBSCRIBE_TOPIC))
    Serial.print("ESP32 - Subscribed to the topic: ");
  else
    Serial.print("ESP32 - Failed to subscribe to the topic: ");

  Serial.println(SUBSCRIBE_TOPIC);
  Serial.println("ESP32  - MQTT broker Connected!");
}
void sendToMQTT(const char *filename) {
  StaticJsonDocument<200> message;
  message["timestamp"] = millis();
  message["data"] = filename;  // Or you can read data from other sensors
  char messageBuffer[512];
  serializeJson(message, messageBuffer);

  mqtt.publish(PUBLISH_TOPIC, messageBuffer);

  // Serial.println("ESP32 - sent to MQTT:");
  // Serial.print("- topic: ");
  // Serial.println(PUBLISH_TOPIC);
  // Serial.print("- payload:");
  // Serial.println(messageBuffer);
}
void messageHandler(String &topic, String &payload) {
  Serial.println("ESP32 - received from MQTT:");
  Serial.println("- topic: " + topic);
  Serial.println("- payload:");
  Serial.println(payload);

  // You can process the incoming data as json object, then control something

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.println("Failed to deserialize JSON!");
    return;
  }

  const char *setConfig = doc["setConfig"];
  Serial.printf("config: %s \n", setConfig);
  if (doc["setConfig"] == "SetUrlDownload") {
    const char *url = doc["url"];
    const char *fileName = doc["fileName"];

    // Sử dụng String để tạo đường dẫn file
    String filePath = "/" + String(fileName);

    Serial.printf("url: %s \n", url);
    Serial.printf("filePath: %s \n", filePath.c_str());
    File Image = SD.open(filePath, FILE_READ);
    if (!Image) {
      downloadImage(url, filePath.c_str());
    } else {
      Serial.println("filename already exists");
    }
    // downloadImage(url, fileName);
  }
}
void downloadImage(const char *url, const char *filePath) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, cannot download image.");
    return;
  }

  HTTPClient http;
  http.begin(url);

  int httpCode = http.GET();
  int size = http.getSize();

  if (httpCode == 200) {
    File file = SD.open(filePath, FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open file for writing!");
      http.end();
      return;
    }

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[512];  // Buffer đọc dữ liệu
    int totalBytes = 0;
    int bytesRead;

    Serial.println("Downloading image...");
    while (http.connected() && (size > 0 || size == -1)) {
      size_t streamSize = stream->available();
      if (streamSize) {
        int c = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
        file.write(buffer, c);  // Ghi dữ liệu vào thẻ SD

        if (size > 0) {
          size -= c;
          totalBytes += c;
        }
      }
      delay(1);
    }
    file.close();


    if (totalBytes > 0) {
      Serial.println("Download complete! File saved.");
      delay(2000);
      ESP.restart();
    } else {
      Serial.println("Download failed: No data received.");
      Serial.printf("Deleting file: %s\n", filePath);
      SD.remove(filePath);
    }
  } else {
    Serial.printf("HTTP request failed, error: %d\n", httpCode);
  }

  http.end();
}

void touch_calibrate() {
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // check file system exists
  if (!SPIFFS.begin()) {
    Serial.println("formatting file system");
    SPIFFS.format();
    SPIFFS.begin();
  }

  // check if calibration file exists and size is correct
  if (SPIFFS.exists(CALIBRATION_FILE)) {
    if (REPEAT_CAL) {
      // Delete if we want to re-calibrate
      SPIFFS.remove(CALIBRATION_FILE);
    } else {
      File f = SPIFFS.open(CALIBRATION_FILE, "r");
      if (f) {
        if (f.readBytes((char *)calData, 14) == 14)
          calDataOK = 1;
        f.close();
      }
    }
  }

  if (calDataOK && !REPEAT_CAL) {
    // calibration data valid
    tft.setTouch(calData);
  } else {
    // data not valid so recalibrate
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.println("Touch corners as indicated");

    tft.setTextFont(1);
    tft.println();

    if (REPEAT_CAL) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Set REPEAT_CAL to false to stop this running again!");
    }

    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Calibration complete!");

    // store data
    File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calData, 14);
      f.close();
    }
  }
}
