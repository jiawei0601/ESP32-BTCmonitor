#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- WiFi 設定 ---
const char* ssid = "ASUS-501";
const char* password = "dymj1r5i";

// --- Binance API URL ---
// 獲取最近 30 根 1小時 K線
const String klineUrl = "https://api.binance.com/api/v3/klines?symbol=BTCUSDT&interval=1h&limit=30";

TFT_eSPI tft = TFT_eSPI();

struct KLine {
    float open;
    float high;
    float low;
    float close;
};

KLine klines[30];
float currentPrice = 0;

void connectWiFi() {
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Connecting to WiFi...", 160, 100, 2);
    
    WiFi.begin(ssid, password);
    int counter = 0;
    while (WiFi.status() != WL_CONNECTED && counter < 20) {
        delay(500);
        Serial.print(".");
        counter++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN);
        tft.drawString("WiFi Connected!", 160, 100, 2);
        Serial.println("\nWiFi Connected");
    } else {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED);
        tft.drawString("WiFi Failed!", 160, 100, 2);
        Serial.println("\nWiFi Failed");
    }
    delay(1000);
}

void fetchKLineData() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(klineUrl);
        int httpCode = http.GET();
        
        if (httpCode > 0) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            
            if (!error) {
                JsonArray arr = doc.as<JsonArray>();
                for (int i = 0; i < arr.size() && i < 30; i++) {
                    klines[i].open = arr[i][1].as<float>();
                    klines[i].high = arr[i][2].as<float>();
                    klines[i].low = arr[i][3].as<float>();
                    klines[i].close = arr[i][4].as<float>();
                }
                currentPrice = klines[29].close;
                Serial.printf("Price: %.2f\n", currentPrice);
            }
        }
        http.end();
    }
}

// --- 繪圖邏輯 ---
void drawKLines() {
    int chartX = 10;
    int chartY = 180; // K線圖底部座標
    int chartHeight = 80;
    int barWidth = 8;
    int spacing = 2;
    
    // 找出 30 根 K線中的最高與最低價，用於縮放比例
    float maxH = -1;
    float minL = 1000000;
    for (int i = 0; i < 30; i++) {
        if (klines[i].high > maxH) maxH = klines[i].high;
        if (klines[i].low < minL) minL = klines[i].low;
    }
    
    float range = maxH - minL;
    if (range == 0) range = 1;
    
    // 清除 K線區域
    tft.fillRect(0, 100, 320, 100, TFT_BLACK);
    
    for (int i = 0; i < 30; i++) {
        int x = chartX + i * (barWidth + spacing);
        
        // 映射價格到像素座標
        int yOpen = chartY - (int)((klines[i].open - minL) / range * chartHeight);
        int yClose = chartY - (int)((klines[i].close - minL) / range * chartHeight);
        int yHigh = chartY - (int)((klines[i].high - minL) / range * chartHeight);
        int yLow = chartY - (int)((klines[i].low - minL) / range * chartHeight);
        
        uint32_t color = (klines[i].close >= klines[i].open) ? TFT_GREEN : TFT_RED;
        
        // 畫最高最低線 (Wick)
        tft.drawLine(x + barWidth/2, yHigh, x + barWidth/2, yLow, color);
        
        // 畫實體 (Body)
        int bodyH = abs(yOpen - yClose);
        if (bodyH == 0) bodyH = 1;
        tft.fillRect(x, min(yOpen, yClose), barWidth, bodyH, color);
    }
}

void drawUI() {
    tft.fillScreen(TFT_BLACK);
    
    // Header
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("BTC/USDT (1h)", 10, 10, 2);
    
    // 當前價格
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW);
    char priceStr[20];
    sprintf(priceStr, "$ %.1f", currentPrice);
    tft.drawString(priceStr, 160, 50, 4);
    
    // 繪製 K線圖
    drawKLines();
    
    tft.setTextDatum(BL_DATUM);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("Last Updated: " + String(millis()/1000) + "s", 10, 230, 1);
}

void setup() {
    Serial.begin(115200);
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    
    connectWiFi();
    fetchKLineData();
    drawUI();
}

void loop() {
    static unsigned long lastTick = 0;
    if (millis() - lastTick > 60000) { // 每分鐘更新一次
        fetchKLineData();
        drawUI();
        lastTick = millis();
    }
}
