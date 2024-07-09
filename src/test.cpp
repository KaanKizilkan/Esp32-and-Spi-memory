#include <SPI.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
// Harici SPI Flash için bağlantı pinleri
#define FLASH_CS 5
#define FLASH_CLK 18
#define FLASH_MISO 19
#define FLASH_MOSI 23
#define STAT_WIP 1
#define CURRENT_PAGE_FILE "/current_page.txt"

unsigned char flash_wait_for_write = 0;

unsigned char flash_read_status(void)
{
  unsigned char c;

// This can't do a write_pause
  digitalWrite(FLASH_CS, LOW);  
  SPI.transfer(0x05);
  c = SPI.transfer(0x00);
  digitalWrite(FLASH_CS, HIGH);
  return(c);
}


void write_pause(void)
{
  if(flash_wait_for_write) {
    while(flash_read_status() & STAT_WIP);
    flash_wait_for_write = 0;
  }
}


void save_current_page(int page) {
  File file = LittleFS.open(CURRENT_PAGE_FILE, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  file.println(page);
  file.close();
}

int load_current_page() {
  File file = LittleFS.open(CURRENT_PAGE_FILE, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return 0; // Varsayılan değer
  }

  String pageString = file.readStringUntil('\n');
  file.close();
 
  int page = pageString.toInt();
  return page;

    
}





// convert a page number to a 24-bit address
int page_to_address(int pn)
{
  return(pn << 8);
}


//////////////////////
void flash_page_program(const char* fileName) {
  unsigned char buffer[256];
  int address;
  int current_page = load_current_page(); // Başlangıç sayfa numarası

  // Dosya içeriğini oku
  File file = LittleFS.open(fileName, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  while (file.available()) {
    int bytesRead = file.readBytesUntil('\n', buffer, sizeof(buffer));
    if (bytesRead > 0 && buffer[bytesRead - 1] == '\r') {
      buffer[--bytesRead] = '\0'; // Carriage return karakterini kaldır
    }
    buffer[bytesRead] = '\0'; // Null-terminate the string

    // HEX string'i binary veriye dönüştür
    int binaryLength = 0;
    for (int i = 0; i < bytesRead; i += 2) {
      char hex[3] = { buffer[i], buffer[i + 1], '\0' };
      buffer[binaryLength++] = (unsigned char) strtoul(hex, nullptr, 16);
    }

    write_pause();

    

    // Send Write Enable command
    digitalWrite(FLASH_CS, LOW);
    SPI.transfer(0x06);
    digitalWrite(FLASH_CS, HIGH);
    if (current_page == 0) current_page = 1;

    // Send Page Program command
    digitalWrite(FLASH_CS, LOW);
    SPI.transfer(0x02);

    // Send the 3 byte address
    address = page_to_address(current_page);
    SPI.transfer((address >> 16) & 0xFF);
    SPI.transfer((address >> 8) & 0xFF);
    SPI.transfer(address & 0xFF);

    // Now write 256 bytes to the page
    for (int i = 0; i < binaryLength; i++) {
      SPI.transfer(buffer[i]);
    }

    digitalWrite(FLASH_CS, HIGH);

    // Indicate that next I/O must wait for this write to finish
    flash_wait_for_write = 1;

    
  }
  
  current_page++;

  String filePath = "/" + String(fileName);
  LittleFS.remove(filePath);


  file.close();
  save_current_page(current_page);
  Serial.println("yazma islemi tamamlandi");
}



void writeEnable() {
  digitalWrite(FLASH_CS, LOW);
  SPI.transfer(0x06); // Write Enable
  digitalWrite(FLASH_CS, HIGH);
}


String flash_read_pages(int page) {
  unsigned char buffer[256];
  int address;
  unsigned char *rp = buffer;
  if(page==0){
    return "değer yok";
  }
  write_pause();
  digitalWrite(FLASH_CS, LOW);
  SPI.transfer(0x03);
  // Send the 3 byte address
  address = page_to_address(page);
  SPI.transfer((address >> 16) & 0xFF);
  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer(address & 0xFF);
  // Now read the page's data bytes
  for(uint16_t i = 0; i < 256; i++) {
    yield();
    *rp++ = SPI.transfer(0);
  }
  digitalWrite(FLASH_CS, HIGH);

  // Veriyi HEX formatına dönüştür ve String olarak döndür
  String output;
  for (int i = 0; i < 256; i++) {
    char hexValue[3];
    sprintf(hexValue, "%02X", buffer[i]);
    output += hexValue;
    if (i % 16 == 15) output += "\n"; // 16 byte'lık satırları ayırmak için
    else output += " ";
  }

  return output;
}




String eraseSector() {
  // Yazma etkinleştirme komutunu gönderin
  digitalWrite(FLASH_CS, LOW);
  SPI.transfer(0x06); // Write Enable (WREN) komutu
  digitalWrite(FLASH_CS, HIGH);
  delay(10);

  int address=0x00000000;
  // Sektör silme komutunu gönderin
  digitalWrite(FLASH_CS, LOW);
  SPI.transfer(0x20);
  SPI.transfer((address >> 16) & 0xFF); // Adresin en üst 8 biti
  SPI.transfer((address >> 8) & 0xFF);  // Adresin orta 8 biti
  SPI.transfer(address & 0xFF);         // Adresin en alt 8 biti
  digitalWrite(FLASH_CS, HIGH);
  delay(10);

  // Yazma işleminin bitmesini bekleyin
  while (flash_read_status() & 0x01) {  // Write In Progress (WIP) bitini kontrol edin
    delay(10);
  }
  save_current_page(0);
  return "Sector erased successfully";
}


