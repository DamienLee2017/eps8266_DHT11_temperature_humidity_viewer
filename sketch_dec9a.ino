/*
  作者：谁是以实玛利/RUA55
  店铺：Telesky旗舰店
  功能：采集温湿度数据，每分钟上传到Apache IoTDB
  硬件：ESP8266/ESP32 + DHT11
*/

#include <DHT.h>
//#include <WiFi.h>  // ESP32用这个
//#include <HTTPClient.h> // ESP32用这个
#include <ESP8266WiFi.h>  // ESP8266用这个
#include <ESP8266HTTPClient.h> 

#include <NTPClient.h>
#include <WiFiUdp.h>

#define DHTPIN 2         // 连接 DHT11 数据引脚的数字引脚D4
#define DHTTYPE DHT11    // 定义使用的 DHT 类型是 DHT11

// WiFi配置
const char* ssid = "xiangyu501_5G_plus";
const char* password = "xiangyu501";

// IoTDB配置
const char* iotdb_host = "192.168.2.35";
const int iotdb_port = 6667;  // 如果使用REST API，端口可能是8086
const char* iotdb_url = "http://192.168.2.35:18080/rest/v2/insertTablet";
// 注意：18080是IoTDB REST API端口，需要先启动REST服务

DHT dht(DHTPIN, DHTTYPE);
unsigned long lastUploadTime = 0;
const unsigned long uploadInterval = 60000;  // 60秒 = 1分钟


//WiFiClient client; // 用于HTTP连接
WiFiUDP ntpUDP; // 用于NTP协议
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // 定义全局NTP客户端

void setup() {
  Serial.begin(115200);
  dht.begin();
  
  // 连接WiFi
  connectToWiFi();
  
// 在连接WiFi之后，进行NTP同步
timeClient.begin();
Serial.println("开始尝试同步NTP时间...");

int maxRetries = 10; // 最大重试次数
int retryCount = 0;
while (retryCount < maxRetries) {
    if (timeClient.update()) {
        Serial.println("NTP时间同步成功！");
        Serial.print("当前NTP时间（格式化）: ");
        Serial.println(timeClient.getFormattedTime()); // 打印如 "15:45:30"
        Serial.print("当前NTP时间戳（秒）: ");
        Serial.println(timeClient.getEpochTime()); // 打印秒级时间戳，这个值现在应该很大（十位数）
        break; // 成功则跳出循环
    } else {
        retryCount++;
        Serial.print("NTP同步失败，正在重试 (");
        Serial.print(retryCount);
        Serial.println("/10)...");
        delay(2000); // 等待2秒后重试
    }
}
if (retryCount >= maxRetries) {
    Serial.println("错误：NTP同步失败，请检查网络！");
}

  Serial.println("系统初始化完成，开始采集数据...");
}

void loop() {
  // 每分钟读取一次数据
  if (millis() - lastUploadTime >= uploadInterval) {
    readAndUploadData();
    lastUploadTime = millis();
  }
  
  // 保持WiFi连接
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }
  
  delay(1000);  // 短暂延迟，避免过度循环
}

void connectToWiFi() {
  Serial.print("正在连接WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi连接成功!");
    Serial.print("IP地址: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi连接失败!");
  }
}

void readAndUploadData() {
  // 读取传感器数据
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  // 检查是否读取成功
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("读取传感器失败！");
    return;
  }
  
  // 打印到串口
  Serial.print("温度: ");
  Serial.print(temperature);
  Serial.print(" °C, 湿度: ");
  Serial.print(humidity);
  Serial.println(" %");
  
  // 上传到IoTDB
  uploadToIoTDB(temperature, humidity);
}

void uploadToIoTDB(float temperature, float humidity) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi未连接，无法上传数据");
        return;
    }

    WiFiClient client;
    HTTPClient http;

    // 使用 REST v2 的 insertTablet 接口
    String url = "http://192.168.2.35:18080/rest/v2/insertTablet";
    if (!http.begin(client, url)) {
        Serial.println("连接HTTP服务器失败！");
        return;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Basic cm9vdDpyb290"); // root:root的base64编码

    // 获取当前时间戳（毫秒）。如果设备有时间同步，这里应替换为真实的Unix时间戳。

  // 【关键】从全局的 timeClient 对象获取时间
      // 1. 在获取时间前，先强制更新一次NTP客户端，确保时间最新
    if (!timeClient.update()) {
        timeClient.forceUpdate(); // 如果常规更新失败，尝试强制更新
    }
    // 2. 使用 64 位（unsigned long long）变量来存储时间戳，避免溢出
    unsigned long long currentEpochSeconds = timeClient.getEpochTime();
    unsigned long long epochMillis = currentEpochSeconds * 1000ULL; // 注意使用 ULL 后缀

    // !!! 强烈建议：添加调试输出，验证此时获取的时间 !!!
    Serial.print("[调试] 上传函数内获取的秒时间戳(64位): ");
    Serial.println(currentEpochSeconds);
    Serial.print("[调试] 转换后的毫秒时间戳(64位): ");
    Serial.println(epochMillis);
    // 同时打印一个32位版本用于对比
    Serial.print("[对比] 截断为32位的毫秒戳: ");
    Serial.println((unsigned long)epochMillis); 

    
    // 构建符合 insertTablet 接口要求的 JSON 数据体
    // 关键：timestamps 是数组，values 是二维数组（列优先）
 String jsonPayload = "{";
jsonPayload += "\"timestamps\":[" + String(epochMillis) + "],";
jsonPayload += "\"measurements\":[\"temperature\",\"humidity\"],";
jsonPayload += "\"data_types\":[\"FLOAT\",\"FLOAT\"],"; // 注意改为下划线
jsonPayload += "\"values\":[[" + String(temperature) + "],[" + String(humidity) + "]],";
jsonPayload += "\"is_aligned\": false,"; // <-- 关键：添加此行，对于普通设备设为 false
jsonPayload += "\"device\":\"root.sensor.dht11\""; // 将 deviceId 改为 device
jsonPayload += "}";

    Serial.println("发送数据 (insertTablet): " + jsonPayload);

    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
        Serial.print("HTTP状态码: ");
        Serial.println(httpResponseCode);
        if (httpResponseCode == 200) {
            Serial.println("✅ 数据写入成功！");
        }
        String response = http.getString();
        Serial.println("服务器响应: " + response);
    } else {
        Serial.print("POST请求失败，错误代码: ");
        Serial.println(httpResponseCode);
    }
    http.end();
}