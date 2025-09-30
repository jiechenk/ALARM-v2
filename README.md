# ALARM-v2 BY JIE

Upload File HTML ke LittleFS:
Install plugin "ESP32 Sketch Data Upload" atau gunakan tool upload LittleFS
Buat folder data di folder sketch Arduino
Simpan file HTML Anda sebagai gabungan2.html di folder data
Upload ke ESP32 menggunakan menu Tools > ESP32 Sketch Data Upload
Library yang diperlukan:
WiFi (built-in)
WebServer (built-in)
Preferences (built-in)
ArduinoJson (install via Library Manager)
LittleFS (built-in)
Pin Relay: GPIO 4, 5, 18, 19 (sesuaikan dengan hardware Anda)



WiFi Setup:

1 AP MODE
Pertama kali akan membuat AP: Futsal_Timer_Setup password 12345678
Akses via IP: 192.168.4.1

2 CLIEN  MODE
Atur WiFi di halaman pengaturan
ESP32 akan restart dan connect ke WiFi yang disimpan
ESP32 terhubung ke WiFi router, ada beberapa cara untuk mengaksesnya:

 1: Melihat IP Address dari Serial Monitor (Paling Mudah)
 Buka Serial Monitor di Arduino IDE (115200 baud)
 Setelah terhubung ke WiFi, akan muncul:
  WiFi Connected!
  IP Address: 192.168.1.xxx

 2: Menggunakan mDNS (Hostname) Kemudian akses dengan: http://futsal-timer.local
