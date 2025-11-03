#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Wire.h>
#include <Adafruit_FT6206.h>

// ========================================================
//  KONFIGURASI TFT ILI9488 (SPI) untuk ESP32-S3
// ========================================================
class LGFX_ESP32S3_ILI9488 : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX_ESP32S3_ILI9488(void) {
    { 
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 12;
      cfg.pin_mosi = 11;
      cfg.pin_miso = 13;
      cfg.pin_dc   = 14;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    { 
      auto cfg = _panel_instance.config();
      cfg.pin_cs   = -1;
      cfg.pin_rst  = 10;
      cfg.pin_busy = -1;
      cfg.memory_width  = 320;
      cfg.memory_height = 480;
      cfg.panel_width   = 320;
      cfg.panel_height  = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.invert   = false;
      cfg.readable = true;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

// ========================================================
//  INSTANCE GLOBAL
// ========================================================
LGFX_ESP32S3_ILI9488 tft;
LGFX_Sprite sprite(&tft);
Adafruit_FT6206 touch = Adafruit_FT6206();

// ========================================================
//  KALIBRASI VARIABLES
// ========================================================
struct CalibrationPoint {
  int screenX, screenY;
  int rawX, rawY;
  bool captured;
  String name;
  uint16_t color;
};

CalibrationPoint calibPoints[4];
int currentPoint = 0;
bool calibrationDone = false;

// Hasil kalibrasi
int TOUCH_MAP_X1 = 0, TOUCH_MAP_X2 = 240;
int SCREEN_MAP_X1 = 0, SCREEN_MAP_X2 = 320;
int TOUCH_MAP_Y1 = 0, TOUCH_MAP_Y2 = 320;
int SCREEN_MAP_Y1 = 0, SCREEN_MAP_Y2 = 480;

// ========================================================
//  FUNGSI INISIALISASI CALIBRATION POINTS
// ========================================================
void initCalibrationPoints() {
  // Top-Left
  calibPoints[0] = {20, 20, 0, 0, false, "Top-Left", TFT_RED};
  
  // Top-Right
  calibPoints[1] = {460, 20, 0, 0, false, "Top-Right", TFT_GREEN};
  
  // Bottom-Left
  calibPoints[2] = {20, 300, 0, 0, false, "Bottom-Left", TFT_BLUE};
  
  // Bottom-Right
  calibPoints[3] = {460, 300, 0, 0, false, "Bottom-Right", TFT_YELLOW};
}

// ========================================================
//  FUNGSI GAMBAR CROSSHAIR
// ========================================================
void drawCrosshair(int x, int y, uint16_t color, bool active = false) {
  int size = active ? 25 : 20;
  int thickness = active ? 3 : 2;
  
  // Outer circle
  if (active) {
    sprite.drawCircle(x, y, size + 5, color);
    sprite.drawCircle(x, y, size + 6, color);
  }
  
  // Cross lines
  sprite.fillRect(x - size, y - thickness/2, size*2, thickness, color);
  sprite.fillRect(x - thickness/2, y - size, thickness, size*2, color);
  
  // Center circle
  sprite.fillCircle(x, y, 5, color);
  sprite.drawCircle(x, y, 5, TFT_WHITE);
  
  // Pulsing effect for active
  if (active) {
    static unsigned long lastPulse = 0;
    static int pulseSize = 0;
    if (millis() - lastPulse > 100) {
      pulseSize = (pulseSize + 1) % 3;
      lastPulse = millis();
    }
    sprite.drawCircle(x, y, 8 + pulseSize, color);
  }
}

// ========================================================
//  FUNGSI GAMBAR UI KALIBRASI
// ========================================================
void drawCalibrationUI() {
  sprite.fillScreen(0x1082); // Dark blue
  
  // Title bar
  sprite.fillRect(0, 0, 480, 50, TFT_NAVY);
  sprite.setFont(&fonts::Font4);
  sprite.setTextDatum(textdatum_t::middle_center);
  sprite.setTextColor(TFT_CYAN);
  sprite.drawString("TOUCH SCREEN CALIBRATION", 240, 25);
  
  // Draw all crosshairs
  for (int i = 0; i < 4; i++) {
    bool isActive = (i == currentPoint && !calibrationDone);
    uint16_t color = calibPoints[i].captured ? TFT_DARKGREY : calibPoints[i].color;
    drawCrosshair(calibPoints[i].screenX, calibPoints[i].screenY, color, isActive);
  }
  
  // Instructions
  sprite.setFont(&fonts::Font2);
  sprite.setTextColor(TFT_WHITE);
  sprite.setTextDatum(textdatum_t::middle_center);
  
  if (!calibrationDone) {
    String instruction = "Touch the " + calibPoints[currentPoint].name + " crosshair";
    sprite.drawString(instruction, 240, 160);
    
    sprite.setTextColor(TFT_YELLOW);
    sprite.drawString("Step " + String(currentPoint + 1) + " of 4", 240, 190);
  } else {
    sprite.setFont(&fonts::Font4);
    sprite.setTextColor(TFT_GREEN);
    sprite.drawString("CALIBRATION COMPLETE!", 240, 160);
    
    sprite.setFont(&fonts::Font2);
    sprite.setTextColor(TFT_WHITE);
    sprite.drawString("Touch anywhere to test", 240, 190);
  }
  
  // Progress bar
  int progressWidth = 400;
  int progressX = 40;
  int progressY = 220;
  sprite.drawRect(progressX, progressY, progressWidth, 20, TFT_WHITE);
  int filled = (currentPoint * progressWidth) / 4;
  sprite.fillRect(progressX + 2, progressY + 2, filled, 16, TFT_GREEN);
  
  // Status info
  sprite.setFont(&fonts::Font2);
  sprite.setTextColor(TFT_LIGHTGREY);
  sprite.setTextDatum(textdatum_t::top_left);
  sprite.drawString("Screen: 480x320 (Landscape)", 20, 250);
  sprite.drawString("Touch Controller: FT6236", 20, 270);
  
  sprite.pushSprite(0, 0);
}

// ========================================================
//  FUNGSI TAMPILKAN HASIL KALIBRASI
// ========================================================
void displayCalibrationResults() {
  sprite.fillScreen(TFT_BLACK);
  
  // Title
  sprite.setFont(&fonts::Font4);
  sprite.setTextDatum(textdatum_t::middle_center);
  sprite.setTextColor(TFT_GREEN);
  sprite.drawString("CALIBRATION RESULTS", 240, 30);
  
  // Raw coordinates
  sprite.setFont(&fonts::Font2);
  sprite.setTextColor(TFT_CYAN);
  sprite.setTextDatum(textdatum_t::top_left);
  sprite.drawString("RAW COORDINATES:", 20, 70);
  
  for (int i = 0; i < 4; i++) {
    sprite.setTextColor(calibPoints[i].color);
    char buffer[64];
    sprintf(buffer, "%s: X=%3d, Y=%3d", 
            calibPoints[i].name.c_str(), 
            calibPoints[i].rawX, 
            calibPoints[i].rawY);
    sprite.drawString(buffer, 40, 95 + i * 20);
  }
  
  // Calculated mapping
  sprite.setTextColor(TFT_YELLOW);
  sprite.drawString("CALCULATED MAPPING:", 20, 190);
  
  sprite.setTextColor(TFT_WHITE);
  char mapBuffer[128];
  sprintf(mapBuffer, "X: %d -> %d  =>  %d -> %d", 
          TOUCH_MAP_X1, TOUCH_MAP_X2, SCREEN_MAP_X1, SCREEN_MAP_X2);
  sprite.drawString(mapBuffer, 40, 215);
  
  sprintf(mapBuffer, "Y: %d -> %d  =>  %d -> %d", 
          TOUCH_MAP_Y1, TOUCH_MAP_Y2, SCREEN_MAP_Y1, SCREEN_MAP_Y2);
  sprite.drawString(mapBuffer, 40, 235);
  
  // Code to copy
  sprite.setTextColor(TFT_CYAN);
  sprite.drawString("COPY THIS CODE:", 250, 70);
  
  sprite.setTextColor(TFT_GREEN);
  sprite.setFont(&fonts::Font0);
  sprite.setTextDatum(textdatum_t::top_left);
  
  sprite.drawString("int TOUCH_MAP_X1 = " + String(TOUCH_MAP_X1) + ";", 260, 95);
  sprite.drawString("int TOUCH_MAP_X2 = " + String(TOUCH_MAP_X2) + ";", 260, 110);
  sprite.drawString("int SCREEN_MAP_X1 = " + String(SCREEN_MAP_X1) + ";", 260, 125);
  sprite.drawString("int SCREEN_MAP_X2 = " + String(SCREEN_MAP_X2) + ";", 260, 140);
  sprite.drawString("int TOUCH_MAP_Y1 = " + String(TOUCH_MAP_Y1) + ";", 260, 155);
  sprite.drawString("int TOUCH_MAP_Y2 = " + String(TOUCH_MAP_Y2) + ";", 260, 170);
  sprite.drawString("int SCREEN_MAP_Y1 = " + String(SCREEN_MAP_Y1) + ";", 260, 185);
  sprite.drawString("int SCREEN_MAP_Y2 = " + String(SCREEN_MAP_Y2) + ";", 260, 200);
  
  // Info
  sprite.setFont(&fonts::Font2);
  sprite.setTextColor(TFT_DARKGREY);
  sprite.setTextDatum(textdatum_t::middle_center);
  sprite.drawString("Touch screen to test calibration", 240, 280);
  sprite.drawString("Check Serial Monitor for full code", 240, 300);
  
  sprite.pushSprite(0, 0);
  
  // Print to serial
  Serial.println("\n╔════════════════════════════════════════════════╗");
  Serial.println("║     TOUCH CALIBRATION RESULTS                 ║");
  Serial.println("╚════════════════════════════════════════════════╝\n");
  
  Serial.println("// Copy this code to your program:");
  Serial.println("const int TOUCH_MAP_X1 = " + String(TOUCH_MAP_X1) + ";");
  Serial.println("const int TOUCH_MAP_X2 = " + String(TOUCH_MAP_X2) + ";");
  Serial.println("const int SCREEN_MAP_X1 = " + String(SCREEN_MAP_X1) + ";");
  Serial.println("const int SCREEN_MAP_X2 = " + String(SCREEN_MAP_X2) + ";");
  Serial.println("const int TOUCH_MAP_Y1 = " + String(TOUCH_MAP_Y1) + ";");
  Serial.println("const int TOUCH_MAP_Y2 = " + String(TOUCH_MAP_Y2) + ";");
  Serial.println("const int SCREEN_MAP_Y1 = " + String(SCREEN_MAP_Y1) + ";");
  Serial.println("const int SCREEN_MAP_Y2 = " + String(SCREEN_MAP_Y2) + ";\n");
  
  Serial.println("// Mapping function:");
  Serial.println("void getTouchCoordinates(int &screenX, int &screenY) {");
  Serial.println("  TS_Point p = touch.getPoint();");
  Serial.println("  screenY = map(p.x, TOUCH_MAP_X1, TOUCH_MAP_X2, SCREEN_MAP_X1, SCREEN_MAP_X2);");
  Serial.println("  screenX = map(p.y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, SCREEN_MAP_Y1, SCREEN_MAP_Y2);");
  Serial.println("  screenX = constrain(screenX, 0, 479);");
  Serial.println("  screenY = constrain(screenY, 0, 319);");
  Serial.println("}\n");
}

// ========================================================
//  FUNGSI KALKULASI MAPPING
// ========================================================
void calculateMapping() {
  // Calculate X mapping
  TOUCH_MAP_X1 = (calibPoints[0].rawX + calibPoints[2].rawX) / 2;
  TOUCH_MAP_X2 = (calibPoints[1].rawX + calibPoints[3].rawX) / 2;
  
  // Calculate Y mapping
  TOUCH_MAP_Y1 = (calibPoints[0].rawY + calibPoints[1].rawY) / 2;
  TOUCH_MAP_Y2 = (calibPoints[2].rawY + calibPoints[3].rawY) / 2;
  
  // Determine screen mapping based on orientation
  SCREEN_MAP_X1 = 0;
  SCREEN_MAP_X2 = 479;
  SCREEN_MAP_Y1 = 0;
  SCREEN_MAP_Y2 = 319;
  
  // Auto-detect if X/Y need swapping or reversing
  if (abs(calibPoints[0].rawX - calibPoints[1].rawX) < 50) {
    // X tidak berubah banyak, berarti perlu swap
    Serial.println("Detected: Need to swap X and Y");
  }
  
  if (TOUCH_MAP_X1 > TOUCH_MAP_X2) {
    Serial.println("Detected: X axis is reversed");
    int temp = SCREEN_MAP_X1;
    SCREEN_MAP_X1 = SCREEN_MAP_X2;
    SCREEN_MAP_X2 = temp;
  }
  
  if (TOUCH_MAP_Y1 > TOUCH_MAP_Y2) {
    Serial.println("Detected: Y axis is reversed");
    int temp = SCREEN_MAP_Y1;
    SCREEN_MAP_Y1 = SCREEN_MAP_Y2;
    SCREEN_MAP_Y2 = temp;
  }
}

// ========================================================
//  FUNGSI TEST MODE (setelah kalibrasi)
// ========================================================
void testCalibration() {
  if (touch.touched()) {
    TS_Point p = touch.getPoint();
    
    // Apply calculated mapping
    int screenY = map(p.x, TOUCH_MAP_X1, TOUCH_MAP_X2, SCREEN_MAP_X1, SCREEN_MAP_X2);
    int screenX = map(p.y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, SCREEN_MAP_Y1, SCREEN_MAP_Y2);
    
    screenX = constrain(screenX, 0, 479);
    screenY = constrain(screenY, 0, 319);
    
    // Draw touch indicator
    sprite.fillCircle(screenX, screenY, 8, TFT_RED);
    sprite.drawCircle(screenX, screenY, 12, TFT_WHITE);
    sprite.drawCircle(screenX, screenY, 13, TFT_WHITE);
    
    // Draw crosshair
    sprite.drawLine(screenX - 20, screenY, screenX + 20, screenY, TFT_YELLOW);
    sprite.drawLine(screenX, screenY - 20, screenX, screenY + 20, TFT_YELLOW);
    
    // Display coordinates
    sprite.fillRect(0, 0, 200, 60, TFT_BLACK);
    sprite.setFont(&fonts::Font2);
    sprite.setTextDatum(textdatum_t::top_left);
    sprite.setTextColor(TFT_CYAN);
    sprite.drawString("Raw: " + String(p.x) + ", " + String(p.y), 10, 10);
    sprite.setTextColor(TFT_GREEN);
    sprite.drawString("Screen: " + String(screenX) + ", " + String(screenY), 10, 30);
    
    sprite.pushSprite(0, 0);
    
    Serial.printf("Raw: (%3d, %3d) -> Screen: (%3d, %3d)\n", p.x, p.y, screenX, screenY);
    
    delay(50);
  }
}

// ========================================================
//  SETUP
// ========================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════════════╗");
  Serial.println("║  ESP32-S3 TOUCH CALIBRATION TOOL             ║");
  Serial.println("╚════════════════════════════════════════════════╝\n");
  
  // Init display
  tft.init();
  tft.setRotation(1); // Landscape
  tft.setBrightness(220);
  sprite.createSprite(480, 320);
  
  // Splash
  sprite.fillScreen(TFT_BLACK);
  sprite.setFont(&fonts::Font7);
  sprite.setTextDatum(textdatum_t::middle_center);
  sprite.setTextColor(TFT_CYAN);
  sprite.drawString("TOUCH", 240, 120);
  sprite.setFont(&fonts::Font4);
  sprite.setTextColor(TFT_WHITE);
  sprite.drawString("CALIBRATION TOOL", 240, 180);
  sprite.pushSprite(0, 0);
  delay(2000);
  
  // Init touch
  Wire.begin(8, 9);
  Wire.setClock(400000);
  
  bool touchOK = false;
  uint8_t thresholds[] = {40, 128, 20, 60};
  
  for(int i = 0; i < 4; i++) {
    if (touch.begin(thresholds[i])) {
      touchOK = true;
      Serial.printf("✓ Touch initialized (threshold: %d)\n", thresholds[i]);
      break;
    }
    delay(100);
  }
  
  if (!touchOK) {
    sprite.fillScreen(TFT_BLACK);
    sprite.setFont(&fonts::Font4);
    sprite.setTextColor(TFT_RED);
    sprite.drawString("TOUCH INIT FAILED!", 240, 160);
    sprite.pushSprite(0, 0);
    Serial.println("✗ Touch initialization failed!");
    while(1) delay(1000);
  }
  
  // Init calibration points
  initCalibrationPoints();
  
  Serial.println("✓ Ready to calibrate!");
  Serial.println("Touch each crosshair in order...\n");
  
  drawCalibrationUI();
}

// ========================================================
//  LOOP
// ========================================================
unsigned long lastTouchTime = 0;
bool inTestMode = false;

void loop() {
  unsigned long now = millis();
  
  if (calibrationDone && inTestMode) {
    // Test mode
    testCalibration();
    return;
  }
  
  if (touch.touched() && now - lastTouchTime > 500) {
    TS_Point p = touch.getPoint();
    
    if (!calibrationDone) {
      // Capture calibration point
      calibPoints[currentPoint].rawX = p.x;
      calibPoints[currentPoint].rawY = p.y;
      calibPoints[currentPoint].captured = true;
      
      Serial.printf("Point %d captured: Raw(%d, %d)\n", 
                    currentPoint + 1, p.x, p.y);
      
      currentPoint++;
      
      if (currentPoint >= 4) {
        calibrationDone = true;
        calculateMapping();
        displayCalibrationResults();
        Serial.println("\n✓ Calibration complete! Touch screen to test.\n");
      } else {
        drawCalibrationUI();
      }
    } else {
      // Enter test mode
      inTestMode = true;
      sprite.fillScreen(TFT_BLACK);
      sprite.setFont(&fonts::Font4);
      sprite.setTextDatum(textdatum_t::middle_center);
      sprite.setTextColor(TFT_GREEN);
      sprite.drawString("TEST MODE", 240, 160);
      sprite.setFont(&fonts::Font2);
      sprite.setTextColor(TFT_WHITE);
      sprite.drawString("Touch anywhere on screen", 240, 200);
      sprite.pushSprite(0, 0);
      delay(1000);
      Serial.println("Entering test mode...\n");
    }
    
    lastTouchTime = now;
  }
  
  // Redraw UI setiap 100ms untuk animasi
  if (!calibrationDone && now % 100 == 0) {
    drawCalibrationUI();
  }
  
  delay(10);
}