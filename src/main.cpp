#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// --- WiFi 設定 ---
const char* ssid = "jwc";
const char* password = "12345678";

// --- 觸控引腳設定 (針對 CYD ESP32-2432S028) ---
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

SPIClass touchSPI = SPIClass(VSPI);
XPT2046_Touchscreen touch(XPT2046_CS, XPT2046_IRQ);

TFT_eSPI tft = TFT_eSPI();

// --- K線資料結構與變數 ---
struct KLine {
    float open;
    float high;
    float low;
    float close;
};

KLine klines[30];
float currentPrice = 0;

// --- 週期設定 ---
const char* intervals[] = {"1m", "5m", "1h", "4h", "1d"};
int currentIntervalIdx = 2; // 預設 1h

struct Button {
    int x, y, w, h;
    const char* label;
};

Button buttons[5];

void initButtons() {
    int btnW = 56;
    int btnH = 28;
    int startX = 10;
    int y = 208;
    int spacing = 6;
    for (int i = 0; i < 5; i++) {
        buttons[i] = {startX + i * (btnW + spacing), y, btnW, btnH, intervals[i]};
    }
}

void connectWiFi() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Connecting to WiFi...", 160, 100, 2);
    WiFi.begin(ssid, password);
    int counter = 0;
    while (WiFi.status() != WL_CONNECTED && counter < 60) {
        delay(500);
        counter++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN);
        tft.drawString("WiFi Connected!", 160, 100, 2);
    } else {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED);
        tft.drawString("WiFi Failed!", 160, 100, 2);
    }
    delay(1000);
}

void fetchKLineData() {
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        String url = "https://api.binance.com/api/v3/klines?symbol=BTCUSDT&interval=" + String(intervals[currentIntervalIdx]) + "&limit=30";
        if (http.begin(client, url)) {
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                JsonDocument doc;
                deserializeJson(doc, payload);
                JsonArray arr = doc.as<JsonArray>();
                for (int i = 0; i < arr.size() && i < 30; i++) {
                    klines[i].open = arr[i][1].as<float>();
                    klines[i].high = arr[i][2].as<float>();
                    klines[i].low = arr[i][3].as<float>();
                    klines[i].close = arr[i][4].as<float>();
                }
                currentPrice = klines[29].close;
            }
            http.end();
        }
    }
}

void drawKLines() {
    int chartX = 15;
    int chartY = 195; 
    int chartHeight = 85; 
    int barWidth = 7;
    int spacing = 3;
    float maxH = -1, minL = 1000000;
    bool hasData = false;
    for (int i = 0; i < 30; i++) {
        if (klines[i].close > 0) {
            if (klines[i].high > maxH) maxH = klines[i].high;
            if (klines[i].low < minL) minL = klines[i].low;
            hasData = true;
        }
    }
    if (!hasData) return;
    float range = maxH - minL;
    if (range == 0) range = 1;
    maxH += range * 0.1; minL -= range * 0.1; range = maxH - minL;

    // 清除 K線區域並畫邊框 (適應 320 寬)
    tft.fillRect(0, 90, 320, 115, TFT_BLACK);
    tft.drawRect(chartX - 5, chartY - chartHeight - 5, 290, chartHeight + 10, TFT_DARKGREY);
    
    for (int i = 0; i < 30; i++) {
        if (klines[i].close == 0) continue;
        int x = chartX + i * (barWidth + spacing);
        int yOpen = chartY - (int)((klines[i].open - minL) / range * chartHeight);
        int yClose = chartY - (int)((klines[i].close - minL) / range * chartHeight);
        int yHigh = chartY - (int)((klines[i].high - minL) / range * chartHeight);
        int yLow = chartY - (int)((klines[i].low - minL) / range * chartHeight);
        uint32_t color = (klines[i].close >= klines[i].open) ? TFT_GREEN : 0xF800;
        tft.drawLine(x + barWidth/2, yHigh, x + barWidth/2, yLow, color);
        int bodyH = abs(yOpen - yClose); if (bodyH == 0) bodyH = 1;
        tft.fillRect(x, min(yOpen, yClose), barWidth, bodyH, color);
    }
    tft.setTextDatum(MR_DATUM); tft.setTextColor(TFT_LIGHTGREY);
    tft.drawFloat(maxH, 1, 315, chartY - chartHeight, 1);
    tft.drawFloat(minL, 1, 315, chartY, 1);
}

void drawButtons() {
    tft.setTextDatum(MC_DATUM);
    for (int i = 0; i < 5; i++) {
        uint32_t bgColor = (i == currentIntervalIdx) ? TFT_BLUE : TFT_DARKGREY;
        tft.fillRect(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h, bgColor);
        tft.setTextColor(TFT_WHITE);
        tft.drawString(buttons[i].label, buttons[i].x + buttons[i].w/2, buttons[i].y + buttons[i].h/2, 2);
    }
}

void drawUI() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE); tft.setTextDatum(TL_DATUM);
    tft.drawString("BTC/USDT (" + String(intervals[currentIntervalIdx]) + ")", 10, 10, 2);
    tft.setTextDatum(MC_DATUM); tft.setTextColor(TFT_YELLOW);
    char priceStr[20]; sprintf(priceStr, "$ %.1f", currentPrice);
    tft.drawString(priceStr, 160, 50, 4); 
    drawKLines();
    drawButtons();
    tft.setTextDatum(BL_DATUM); tft.setTextColor(TFT_DARKGREY);
    tft.drawString("Updated: " + String(millis()/1000) + "s", 10, 238, 1);
}

void handleTouch() {
    if (touch.touched()) {
        TS_Point p = touch.getPoint();
        // 橫向映射 (Rotation 1)
        int tx = map(p.y, 250, 3850, 0, 320); 
        int ty = map(p.x, 350, 3750, 240, 0);
        
        Serial.printf("Raw: x=%d, y=%d | Mapped: x=%d, y=%d\n", p.x, p.y, tx, ty);
        
        for (int i = 0; i < 5; i++) {
            if (tx >= buttons[i].x && tx <= buttons[i].x + buttons[i].w &&
                ty >= buttons[i].y && ty <= buttons[i].y + buttons[i].h) {
                if (currentIntervalIdx != i) {
                    currentIntervalIdx = i;
                    tft.fillRect(0, 0, 320, 80, TFT_BLACK); 
                    tft.setTextColor(TFT_WHITE); tft.setTextDatum(MC_DATUM);
                    tft.drawString("Loading...", 160, 40, 2);
                    fetchKLineData();
                    drawUI();
                    delay(500);
                }
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(21, OUTPUT); digitalWrite(21, HIGH); 
    tft.init(); tft.setRotation(1); tft.fillScreen(TFT_BLACK);
    
    touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touch.begin(touchSPI);
    touch.setRotation(1);
    
    initButtons();
    connectWiFi();
    fetchKLineData();
    drawUI();
}

void loop() {
    handleTouch();
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 60000) {
        fetchKLineData();
        drawUI();
        lastUpdate = millis();
    }
}
