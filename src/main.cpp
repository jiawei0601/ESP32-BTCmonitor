#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// --- WiFi 設定 ---
const char* ssid = "jwc";
const char* password = "12345678";

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
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Connecting to WiFi...", 160, 100, 2);
    
    Serial.printf("Connecting to %s ", ssid);
    WiFi.begin(ssid, password);
    
    int counter = 0;
    // 延長連線等待時間至 30 秒 (60 * 500ms)
    while (WiFi.status() != WL_CONNECTED && counter < 60) {
        delay(500);
        Serial.print(".");
        counter++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN);
        tft.drawString("WiFi Connected!", 160, 100, 2);
        Serial.println("\nWiFi Connected");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED);
        tft.drawString("WiFi Connection Failed!", 160, 100, 2);
        Serial.println("\nWiFi Failed - Check SSID/Password or Signal");
    }
    delay(1000);
}

void fetchKLineData() {
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClientSecure client;
        client.setInsecure(); // 對於公開 API，不驗證憑證指紋以簡化實作
        
        HTTPClient http;
        Serial.println("Fetching data from Binance...");
        
        if (http.begin(client, klineUrl)) {
            int httpCode = http.GET();
            
            if (httpCode == HTTP_CODE_OK) {
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
                    Serial.printf("Price Updated: %.2f\n", currentPrice);
                } else {
                    Serial.print("JSON Parse Error: ");
                    Serial.println(error.c_str());
                }
            } else {
                Serial.printf("HTTP GET Failed, error: %s\n", http.errorToString(httpCode).c_str());
            }
            http.end();
        } else {
            Serial.println("Unable to connect to Binance server");
        }
    } else {
        Serial.println("WiFi not connected, skipping fetch");
    }
}

// --- 繪圖邏輯 ---
void drawKLines() {
    int chartX = 20;
    int chartY = 200; // K線圖底部座標
    int chartHeight = 80;
    int barWidth = 7;
    int spacing = 3;
    
    // 找出有效 K線中的最高與最低價
    float maxH = -1;
    float minL = 1000000;
    bool hasData = false;
    
    for (int i = 0; i < 30; i++) {
        if (klines[i].close > 0) { // 確保是有效數據
            if (klines[i].high > maxH) maxH = klines[i].high;
            if (klines[i].low < minL) minL = klines[i].low;
            hasData = true;
        }
    }
    
    if (!hasData) return;
    
    // 增加上下邊距，避免線條貼齊
    float range = maxH - minL;
    if (range == 0) range = 1;
    maxH += range * 0.1;
    minL -= range * 0.1;
    range = maxH - minL;

    // 清除 K線區域並畫邊框
    tft.fillRect(0, 100, 320, 120, TFT_BLACK);
    tft.drawRect(chartX - 5, chartY - chartHeight - 10, 305, chartHeight + 20, TFT_DARKGREY);
    
    for (int i = 0; i < 30; i++) {
        if (klines[i].close == 0) continue; // 跳過無效數據
        
        int x = chartX + i * (barWidth + spacing);
        
        // 映射價格到像素座標
        int yOpen = chartY - (int)((klines[i].open - minL) / range * chartHeight);
        int yClose = chartY - (int)((klines[i].close - minL) / range * chartHeight);
        int yHigh = chartY - (int)((klines[i].high - minL) / range * chartHeight);
        int yLow = chartY - (int)((klines[i].low - minL) / range * chartHeight);
        
        // 修正顏色編碼 (有些 CYD 板子紅藍反轉，這裡定義為漲綠跌紅)
        uint32_t color = (klines[i].close >= klines[i].open) ? TFT_GREEN : 0xF800; // 0xF800 是純紅
        
        // 畫最高最低線 (Wick)
        tft.drawLine(x + barWidth/2, yHigh, x + barWidth/2, yLow, color);
        
        // 畫實體 (Body)
        int bodyH = abs(yOpen - yClose);
        if (bodyH == 0) bodyH = 1;
        tft.fillRect(x, min(yOpen, yClose), barWidth, bodyH, color);
    }

    // 顯示最高與最低價標籤
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(TFT_LIGHTGREY);
    tft.drawFloat(maxH, 1, 315, chartY - chartHeight, 1);
    tft.drawFloat(minL, 1, 315, chartY, 1);
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
    delay(1000);
    Serial.println("\n--- ESP32 BTC Monitor ---");

    // 強制開啟背光 (針對 CYD ESP32-2432S028)
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH); 
    
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Initialing...", 160, 120, 2);

    connectWiFi();
    
    if (WiFi.status() == WL_CONNECTED) {
        fetchKLineData();
        drawUI();
    }
    
    Serial.println("Setup complete");
}

void loop() {
    static unsigned long lastTick = 0;
    if (millis() - lastTick > 60000) { // 每分鐘更新一次
        fetchKLineData();
        drawUI();
        lastTick = millis();
    }
}
