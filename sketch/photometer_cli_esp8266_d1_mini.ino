#include <ESP8266WiFi.h>
#include <FS.h>
#include <LittleFS.h>

const String SSID = "photometer_cli";
const String pagesPath = "/pages/";
bool connected = false;
const int readingNumbers = 10;
String ip;

// Set web server port number to 80
WiFiServer server(80);


void hasClientConnectedToWiFi() {
  if (WiFi.softAPgetStationNum() > 0) {
    connected = true;
    return;
  }
  connected = false;
}

String extractFromString(String str, String startChars, String endChars) {
  int i = str.indexOf(startChars);
  int j = str.indexOf(endChars);
  String result = str.substring(i + startChars.length(), j);
  return result;
}

int reading() {
  int i = 0;
  int sum = 0;
  while (i < readingNumbers) {
    sum += analogRead(A0) * (3300 / 1024);
    i++;
    delay(1);
  }
  return sum / readingNumbers;
}

// for basic responses
void okResponse(WiFiClient client) {
  String content = "{\"response\" : \"OK\" }";
  String responseHeaders = "HTTP/1.1 200 OK\n";
  responseHeaders += "Access-Control-Allow-Origin: *\n";
  responseHeaders += "Connection: Keep-Alive\n";
  responseHeaders += "Keep-Alive: timeout=10, max=200\n";
  responseHeaders += "Content-type: application/json \n";
  responseHeaders += "Content-Length:" + String(content.length()) + "\n";
  responseHeaders += "\n";
  Serial.println(responseHeaders + "\n");
  client.print(responseHeaders);
  client.print(content);
}

// for files
void okResponse(WiFiClient client, String filePath, String contentType) {
  File file = LittleFS.open(filePath, "r");
  String responseHeaders = "HTTP/1.1 200 OK\n";
  responseHeaders += "Access-Control-Allow-Origin: *\n";
  responseHeaders += "Connection: Keep-Alive\n";
  responseHeaders += "Keep-Alive: timeout=10, max=200\n";
  responseHeaders += "Content-type: " + contentType + " \n";
  responseHeaders += "Content-Length:" + String(file.size()) + "\n";
  responseHeaders += "\n";
  Serial.println(responseHeaders);
  client.print(responseHeaders);
  long count = 0;
  while (count < file.size()) {
    char c = file.read();
    client.print(c);
    count++;
  }
  file.close();
}

// for JSON
void okResponse(WiFiClient client, String content) {
  String responseHeaders = "HTTP/1.1 200 OK\n";
  responseHeaders += "Access-Control-Allow-Origin: *\n";
  responseHeaders += "Connection: Keep-Alive\n";
  responseHeaders += "Keep-Alive: timeout=10, max=200\n";
  responseHeaders += "Content-type: application/json \n";
  responseHeaders += "Content-Length:" + String(content.length()) + "\n";
  responseHeaders += "\n";
  Serial.println(responseHeaders + "\n");
  client.print(responseHeaders);
  client.print(content);
}

void badRequest(WiFiClient client) {
  String content = "{\"response\" : \"Bad Request\"}";
  String responseHeaders = "HTTP/1.1 400 Bad Request \n";
  responseHeaders += "Access-Control-Allow-Origin: *\n";
  responseHeaders += "Content-type: application/json \n";
  responseHeaders += "Content-Length:" + String(content.length()) + "\n";
  responseHeaders += "\n";
  Serial.println(responseHeaders + "\n");
  client.print(responseHeaders + content);
}

void forbidden(WiFiClient client) {
  String content = "{\"response\" : \" Forbidden\"}";
  String responseHeaders = "HTTP/1.1 403 Forbidden \n";
  responseHeaders += "Access-Control-Allow-Origin: *\n";
  responseHeaders += "Content-type: application/json \n";
  responseHeaders += "Content-Length:" + String(content.length()) + "\n";
  responseHeaders += "\n";
  Serial.println(responseHeaders + "\n");
  client.print(responseHeaders + content);
}

void notFoundResponse(WiFiClient client) {
  String content = "{\"response\" : \"Not Found\"}";
  String responseHeaders = "HTTP/1.1 404 Not Found \n";
  responseHeaders += "Access-Control-Allow-Origin: *\n";
  responseHeaders += "Content-type: application/json \n";
  responseHeaders += "Content-Length:" + String(content.length()) + "\n";
  responseHeaders += "\n";
  Serial.println(responseHeaders + "\n");
  client.print(responseHeaders + content);
}

void preFlightResponse(WiFiClient client) {
  String responseHeaders = "HTTP/1.1 204 No Content\n";
  responseHeaders += "Connection: Keep-Alive\n";
  responseHeaders += "Keep-Alive: timeout=10, max=200\n";
  responseHeaders += "Access-Control-Allow-Origin: *\n";
  responseHeaders += "Access-Control-Allow-Methods: POST, GET\n";
  responseHeaders += "Access-Control-Allow-Headers: access-control-allow-origin,content-disposition,content-type\n";
  responseHeaders += "\n";
  Serial.println(responseHeaders + "\n");
  client.print(responseHeaders);
}

long uploadFile(WiFiClient client, String fileName) {
  String path = pagesPath + fileName;
  long bytes = 0;
  if (LittleFS.exists(path))
    LittleFS.remove(path);
  File file = LittleFS.open(path, "w");
  while (client.available()) {
    char c = client.read();
    file.print(c);
    bytes++;
  }
  file.close();
  return bytes;
}

void handleClient(WiFiClient client, String requestHeaders) {
  //preflight
  if (requestHeaders.indexOf("OPTIONS /upload/") != -1) {
    preFlightResponse(client);
    return;
  }
  // upload pages
  if (requestHeaders.indexOf("POST /upload/") != -1) {
    if (requestHeaders.indexOf("text/html") == -1 || requestHeaders.indexOf("filename=") == -1) {
      badRequest(client);
      return;
    }
    String name = extractFromString(requestHeaders, "/upload/", " HTTP");
    long bytes = uploadFile(client, name);
    String content = "{\"bytes\" : " + String(bytes) + "}";
    okResponse(client, content);
    return;
  }
  // delivery html index page
  if (requestHeaders.indexOf("GET / ") != -1) {
    String path = pagesPath + "index.html";
    if (LittleFS.exists(path)) {
      okResponse(client, path, "text/html");
      return;
    }
  }
  // devilery the others page
  if (requestHeaders.indexOf("GET /pages/") != -1) {
    Serial.println("pages");
    String name = extractFromString(requestHeaders, "/pages/", " HTTP");
    String path = pagesPath + name;
    Serial.println(path);
    if (LittleFS.exists(path)) {
      okResponse(client, path, "text/html");
      return;
    }
  }
  // list all pages
  if (requestHeaders.indexOf("GET /list/pages ") != -1) {
    String content = "[";
    File root = LittleFS.open(pagesPath, "r");
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      if (file) {
        content += "\"";
        content += String(file.name());
        content += "\"";
        file = root.openNextFile();
        while (file) {
          content += ", \"";
          content += String(file.name());
          content += "\"";
          file = root.openNextFile();
        }
      }
    }
    content += "]";
    okResponse(client, content);
    return;
  }
  // clear all pages
  if (requestHeaders.indexOf("GET /clear/pages ") != -1) {
    File root = LittleFS.open(pagesPath, "r");
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file) {
        String name = String(file.name());
        String path = pagesPath + name;
        LittleFS.remove(path);
        file = root.openNextFile();
      }
    }
    okResponse(client);
    return;
  }

  //get output
  if (requestHeaders.indexOf("GET /out") != -1) {
    String output = String(reading());
    String content = "{\"millivots\":" + output + "}";
    okResponse(client, content);
    return;
  }

  notFoundResponse(client);
}

void setup() {
  Serial.begin(115200);
  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
    while (true)
      ;
  }
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  if (!WiFi.softAP(SSID,"", 1, 0, 1)) {
    Serial.println("Webserver failed starting...");
    return;
  }
  ip = WiFi.softAPIP().toString();
  Serial.print("Soft-AP IP address = ");
  Serial.println(ip);  // standard ip = 192.168.4.1
  Serial.println("Webserver started...");
  server.begin();
}

void loop() {
  hasClientConnectedToWiFi();
  WiFiClient client = server.available();
  if (client) {
    String headers = "";
    Serial.println("new client connected...");
    // an http request header ends with a blank line
    bool currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        headers += c;
        Serial.print(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request header has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          handleClient(client, headers);
          break;
        }
        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
    // give time to the browser
    delay(500);
    // close the connection:
    client.stop();
    Serial.println("client disconnected...");
  }
  delay(100);
}
