// embedded/voiceedge_console/voiceedge_console.ino
// VoiceEdge Embedded Network Management Console (Arduino Uno)
// Libraries: Adafruit SSD1306, Adafruit GFX
//
// Wiring
//   OLED (I2C 0x3C): SDA->A4, SCL->A5, VCC->5V, GND->GND
//   LEDs (via 220R): Red D4, Yellow D5, Green D6, Blue D7 -> GND
//   Buttons (other leg to GND): START D8, STOP D9, RESTART D10, MAINT D11

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 oled(128, 64, &Wire, -1);

const uint8_t LED_G = 6, LED_B = 7, LED_Y = 5, LED_R = 4;
const uint8_t BTN_PIN[4] = {8, 9, 10, 11};
const char* const BTN_CMD[4] = {"START", "STOP", "RESTART", "MAINT"};

// latest metrics from gateway
char gw[6] = "OFF", plugin[12] = "NONE", mode[8] = "NORMAL", health[9] = "OK";
int calls = 0, latency = 0;
unsigned long packets = 0;

unsigned long lastRx = 0, lastDraw = 0, lastPress[4] = {0, 0, 0, 0};
bool prevBtn[4] = {HIGH, HIGH, HIGH, HIGH};
const unsigned long LINK_TIMEOUT = 3500;  // ms without metrics = link lost

bool linkUp() { return lastRx != 0 && (millis() - lastRx) < LINK_TIMEOUT; }

void setup() {
  Serial.begin(115200);
  pinMode(LED_G, OUTPUT); pinMode(LED_B, OUTPUT);
  pinMode(LED_Y, OUTPUT); pinMode(LED_R, OUTPUT);
  for (uint8_t i = 0; i < 4; i++) pinMode(BTN_PIN[i], INPUT_PULLUP);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    digitalWrite(LED_R, HIGH);  // OLED not found
    for (;;) {}
  }
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println(F("VOICEEDGE"));
  oled.println(F("Waiting for gateway..."));
  oled.display();
}

void copyField(char* dst, size_t n, const char* src) {
  strncpy(dst, src, n - 1);
  dst[n - 1] = 0;
}

// M|ON|2|37|1234|NONE|NORMAL|OK
void parseLine(char* line) {
  char* t = strtok(line, "|");
  if (!t || t[0] != 'M') return;
  char* f[7];
  for (uint8_t i = 0; i < 7; i++) {
    f[i] = strtok(NULL, "|");
    if (!f[i]) return;
  }
  copyField(gw, sizeof(gw), f[0]);
  calls = atoi(f[1]);
  latency = atoi(f[2]);
  packets = strtoul(f[3], NULL, 10);
  copyField(plugin, sizeof(plugin), f[4]);
  copyField(mode, sizeof(mode), f[5]);
  copyField(health, sizeof(health), f[6]);
  lastRx = millis();
}

void readSerial() {
  static char buf[64];
  static uint8_t n = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') { buf[n] = 0; n = 0; parseLine(buf); }
    else if (c != '\r' && n < sizeof(buf) - 1) buf[n++] = c;
  }
}

void readButtons() {
  for (uint8_t i = 0; i < 4; i++) {
    bool cur = digitalRead(BTN_PIN[i]);
    if (prevBtn[i] == HIGH && cur == LOW && millis() - lastPress[i] > 200) {
      lastPress[i] = millis();
      Serial.print(F("C|"));
      Serial.println(BTN_CMD[i]);
    }
    prevBtn[i] = cur;
  }
}

void updateLeds() {
  bool up = linkUp();
  bool fault = !up || strcmp(health, "FAULT") == 0;
  digitalWrite(LED_G, up && strcmp(gw, "ON") == 0);
  digitalWrite(LED_B, up && calls > 0);
  digitalWrite(LED_Y, up && strcmp(health, "HIGHLAT") == 0);
  digitalWrite(LED_R, fault);
}

void draw() {
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.println(F("VOICEEDGE"));
  if (!linkUp()) {
    oled.println();
    oled.println(F("NO GATEWAY LINK"));
    oled.display();
    return;
  }
  oled.print(F("Gateway : ")); oled.println(gw);
  oled.print(F("Calls   : ")); oled.println(calls);
  oled.print(F("Latency : ")); oled.print(latency); oled.println(F(" ms"));
  oled.print(F("Packets : ")); oled.println(packets);
  oled.print(F("Plugin  : ")); oled.println(plugin);
  oled.print(F("Mode    : ")); oled.println(mode);
  oled.print(F("Status  : ")); oled.println(health);
  oled.display();
}

void loop() {
  readSerial();
  readButtons();
  updateLeds();
  if (millis() - lastDraw > 250) { lastDraw = millis(); draw(); }
}
