#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>  // Yerleşik LittleFS kütüphanesini dahil edin
#include <SPI.h>
#include <esp_task_wdt.h>



// Wi-Fi bilgilerinizi girin
const char* ssid = "deneme";
const char* password = "kmk596zxphwm5q7";

String eraseEntireSPIFlash();

void flash_page_program(const char* fileName);
String flash_read_pages(int page);
String eraseSector();
int load_current_page();


// Web sunucusu portu
AsyncWebServer server(80);
void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}
void setup(){
  // Seri portu başlat
  esp_log_level_set("*", ESP_LOG_VERBOSE);
  
  Serial.begin(115200);
   SPI.begin(18, 19, 23, 5);
  esp_task_wdt_init(800, true);  // 30 saniye, panic (reset) modunda
  esp_task_wdt_add(NULL);   
  // Chip Select pinini OUTPUT olarak ayarla
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);

  // LittleFS'yi başlat
  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS. Formatting...");
    // LittleFS'yi formatla ve tekrar başlat
    if(!LittleFS.format()){
      Serial.println("LittleFS format failed!");
      return;
    }
    if(!LittleFS.begin()){
      Serial.println("LittleFS mount after format failed!");
      return;
    }
  }

  // Wi-Fi'ye bağlan
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  listDir(LittleFS, "/", 0);
  // Ana sayfayı sun
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", String(), false);
  });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){},
[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  if(!index){
    Serial.printf("UploadStart: %s\n", filename.c_str());
    // Dosyayı oluştur
    request->_tempFile = LittleFS.open("/" + filename, "w");
  }
  if(len){
    // Veriyi yaz
    request->_tempFile.write(data, len);
  }
  if(final){
    Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
    // Dosyayı kapat
    request->_tempFile.close();
    // Dosya adı ve içeriği ile flash_page_program çağrısı
    flash_page_program(("/" + filename).c_str());
    request->send(200, "text/plain", "File Uploaded and Written to SPI Flash!");
  }
});


  // Dosya silme rotası
  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
    String response = eraseSector();
    Serial.println("Silme tamamlandı");
    request->send(200, "text/plain", response);

  });

   // Mevcut sayfa sayısını dönen endpoint
  server.on("/page_count", HTTP_GET, [](AsyncWebServerRequest *request){
    int pageCount = load_current_page()-1;
    request->send(200, "text/plain", String(pageCount));
  });

  // Belirli bir sayfanın içeriğini dönen endpoint
  server.on("/page", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("number")) {
      int pageNumber = request->getParam("number")->value().toInt();
      String response = flash_read_pages(pageNumber);
      request->send(200, "text/plain", response);
    } else {
      request->send(400, "text/plain", "Bad Request: page number is missing");
    }
  });

  // Sunucuyu başlat
  server.begin();

 




 
}

void loop(){
  esp_task_wdt_reset();
  delay(10);  // Daha kısa bekleme süresi
  yield(); 

}
