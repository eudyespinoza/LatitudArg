// Definir el tipo de placa LILYGO
#define LILYGO_T_A7670

#include <HardwareSerial.h>
#include <ArduinoJson.h>

// Incluir utilities.h para las definiciones de pines y módem
#include "utilities.h"

// Incluir la librería SSL específica
#include "TinyGsmClientA76xxSSL.h"

#define SerialAT Serial1 // Definido en utilities.h para LILYGO_T_A7670
#define SerialGPS Serial2 // Para el GPS preinstalado (IO19 como RX, IO23 como TX)

// Configuración de Movistar (confirmados como correctos)
const char apn[] = "wap.gprs.unifon.com.ar";
const char gprsUser[] = "wap";
const char gprsPass[] = "wap";
const char simPIN[] = "1234"; // Solo si es necesario
const char server[] = "latitudarg.com";
const char resource[] = "/api/update_location";
const char deviceId[] = "672-AE766";

// Objeto del módem con soporte SSL
TinyGsmA76xxSSL modem(SerialAT);
TinyGsmA76xxSSL::GsmClientSecureA76xxSSL client(modem); // Cliente seguro para HTTPS

// Prototipos de funciones
String sendATCommand(String cmd, String expected, int timeout);
String readATResponse(int timeout);
bool waitForResponse(String expected, int timeout);
void sendLocation(float lat, float lng, float speed);

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("Iniciando prueba de modem... [Hora: 4:40 PM -03, 15/07/2025]");

  // Configurar pines (definidos en utilities.h para LILYGO_T_A7670)
  pinMode(BOARD_PWRKEY_PIN, OUTPUT);
  pinMode(BOARD_POWERON_PIN, OUTPUT);
#ifdef MODEM_RESET_PIN
  pinMode(MODEM_RESET_PIN, OUTPUT);
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL); delay(100);
  digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL); delay(2600);
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
  Serial.println("Modem reseteado");
#endif

  // Encender modem
  Serial.println("Encendiendo modem...");
  digitalWrite(BOARD_POWERON_PIN, HIGH);
  digitalWrite(BOARD_PWRKEY_PIN, LOW);
  delay(100);
  digitalWrite(BOARD_PWRKEY_PIN, HIGH);
  delay(100);
  digitalWrite(BOARD_PWRKEY_PIN, LOW);
  delay(10000);

  // Iniciar comunicación serial (definidos en utilities.h)
  SerialAT.begin(MODEM_BAUDRATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN); // Módem
  SerialGPS.begin(9600, SERIAL_8N1, 19, 23); // GPS preinstalado (IO19 como RX, IO23 como TX)
  delay(10000);

  // Inicializar modem
  Serial.println("Inicializando modem...");
  int retry = 0;
  while (!modem.init()) {
    Serial.println(".");
    if (retry++ > 10) {
      digitalWrite(BOARD_PWRKEY_PIN, LOW);
      delay(100);
      digitalWrite(BOARD_PWRKEY_PIN, HIGH);
      delay(1000);
      digitalWrite(BOARD_PWRKEY_PIN, LOW);
      Serial.println("Reintentando PWRKEY...");
      SerialAT.begin(9600, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
      delay(1000);
      retry = 0;
    }
    delay(1000);
  }
  Serial.println("Modem responde!");

  // Verificar SIM
  if (simPIN[0] != '\0') {
    if (!sendATCommand("AT+CPIN=\"" + String(simPIN) + "\"", "OK", 2000)) {
      Serial.println("Fallo al desbloquear SIM, intentando sin PIN...");
    }
  }
  for (int i = 0; i < 5; i++) {
    if (sendATCommand("AT+CPIN?", "+CPIN: READY", 2000)) {
      Serial.println("SIM lista");
      break;
    }
    Serial.println("Reintentando verificación de SIM...");
    delay(2000);
  }

  // Verificar firmware
  String firmware = modem.getModemInfo();
  Serial.println("Firmware: " + firmware);

  // Configurar contexto PDP
  if (!sendATCommand("AT+CGDCONT=1,\"IP\",\"" + String(apn) + "\"", "OK", 2000)) {
    Serial.println("Fallo al configurar contexto PDP");
    return;
  }

  // Verificar registro de red
  Serial.print("Esperando registro de red...");
  RegStatus status;
  int sq; // Declarar fuera del switch
  while ((status = modem.getRegistrationStatus()) == REG_NO_RESULT || status == REG_SEARCHING || status == REG_UNREGISTERED) {
    switch (status) {
      case REG_UNREGISTERED:
      case REG_SEARCHING:
        sq = modem.getSignalQuality();
        Serial.printf("[%lu] Intensidad de señal: %d\n", millis() / 1000, sq);
        delay(1000);
        break;
      case REG_DENIED:
        Serial.println("Registro de red rechazado, verifica el APN");
        return;
      case REG_OK_HOME:
        Serial.println("Registro de red exitoso (home)");
        break;
      case REG_OK_ROAMING:
        Serial.println("Registro de red exitoso (roaming)");
        break;
      default:
        Serial.printf("Estado de registro: %d\n", status);
        delay(1000);
        break;
    }
  }

  // Conectar GPRS
  Serial.println("Intentando conectar GPRS...");
  int retryCount = 0;
  const int maxRetries = 3;
  while (retryCount < maxRetries && !modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println("Fallo en la conexión GPRS, reintentando... (" + String(retryCount + 1) + "/" + String(maxRetries) + ")");
    delay(5000);
    retryCount++;
  }
  if (modem.isGprsConnected()) {
    Serial.println("GPRS conectado");
  } else {
    Serial.println("No se pudo establecer la conexión GPRS después de " + String(maxRetries) + " intentos");
    return;
  }
}

void loop() {
  String gpsData = getGPSData();
  if (gpsData != "") {
    float lat, lng, speed;
    if (parseGPSData(gpsData, lat, lng, speed)) {
      sendLocation(lat, lng, speed);
    } else {
      Serial.println("No se pudieron parsear las coordenadas GPS");
    }
  } else {
    Serial.println("No se obtuvieron datos GPS");
  }
  delay(10000); // Actualizar cada 5 segundos
}

// Función para obtener datos GPS desde Serial2
String getGPSData() {
  String response = "";
  unsigned long start = millis();
  while (millis() - start < 1000) {
    while (SerialGPS.available()) {
      char c = SerialGPS.read();
      Serial.write(c); // Mostrar datos crudos para depuración
      response += c;
      if (response.endsWith("\r\n")) {
        if (response.startsWith("$GPGGA")) {
          return response; // Retorna la línea $GPGGA
        }
        response = "";
      }
    }
    delay(10);
  }
  return "";
}

// Función para parsear datos GPS
bool parseGPSData(String gpsData, float &lat, float &lng, float &speed) {
  if (!gpsData.startsWith("$GPGGA")) return false;

  int comma1 = gpsData.indexOf(',');
  int comma2 = gpsData.indexOf(',', comma1 + 1);
  int comma3 = gpsData.indexOf(',', comma2 + 1);
  int comma4 = gpsData.indexOf(',', comma3 + 1);
  int comma5 = gpsData.indexOf(',', comma4 + 1);
  int comma6 = gpsData.indexOf(',', comma5 + 1);
  int comma7 = gpsData.indexOf(',', comma6 + 1);

  if (comma2 == -1 || comma4 == -1 || comma6 == -1) return false;

  StringpsData.substring(comma3 + 1, comma4);
  String lngStr = gpsData.substring(comma4 + 1, comma5);
  String lngDir = gpsData.substring(comma5 + 1, comma6);
  String fixQuality = gpsData.substring(comma6 + 1, comma7);

  lat = 0.0;
  lng = 0.0;
  speed = 0.0;

  if (latStr != "" && latStr.length() >= 4) {
    float latDeg = latStr.substring(0, 2).toFloat();
    float latMin = latStr.substring(2).toFloat();
    lat = latDeg + (latMin / 60.0);
    if (latDir == "S") lat = -lat;
  } else {
    lat = 0.0; // Limpiar si no hay datos válidos
    Serial.println("Sin datos de latitud, asignando 0.0");
  }

  if (lngStr != "" && lngStr.length() >= 5) {
    float lngDeg = lngStr.substring(0, 3).toFloat();
    float lngMin = lngStr.substring(3).toFloat();
    lng = lngDeg + (lngMin / 60.0);
    if (lngDir == "W") lng = -lng;
  } else {
    lng = 0.0; // Limpiar si no hay datos válidos
    Serial.println("Sin datos de longitud, asignando 0.0");
  }

  if (fixQuality != "0" && fixQuality != "") {
    String rmcData = "";
    unsigned long start = millis();
    while (millis() - start < 2000) {
      while (SerialGPS.available()) {
        char c = SerialGPS.read();
        Serial.write(c);
        rmcData += c;
        if (rmcData.endsWith("\r\n")) {
          if (rmcData.startsWith("$GPRMC")) {
            break;
          }
          rmcData = "";
        }
      }
      delay(10);
    }
    if (rmcData.startsWith("$GPRMC")) {
      int rmcComma1 = rmcData.indexOf(',');
      int rmcComma7 = rmcData.indexOf(',', rmcData.indexOf(',', rmcData.indexOf(',', rmcData.indexOf(',', rmcData.indexOf(',', rmcData.indexOf(',', 0) + 1) + 1) + 1) + 1) + 1);
      int rmcComma8 = rmcData.indexOf(',', rmcComma7 + 1);
      String status = rmcData.substring(rmcComma7 + 1, rmcComma8);
      if (status == "A") {
        int rmcComma5 = rmcData.indexOf(',', rmcData.indexOf(',', rmcData.indexOf(',', rmcData.indexOf(',', 0) + 1) + 1) + 1);
        String speedStr = rmcData.substring(rmcComma5 + 1, rmcData.indexOf(',', rmcComma5 + 1));
        speed = speedStr.toFloat() * 1.852; // Convertir nudos a km/h
        Serial.println("Velocidad detectada: " + String(speed) + " km/h");
      } else {
        speed = 0.0; // Limpiar si no hay datos válidos
        Serial.println("Sin velocidad válida, asignando 0.0");
      }
    }
  } else {
    lat = 0.0;
    lng = 0.0;
    speed = 0.0;
    Serial.println("Sin fix válido, coordenadas y velocidad asignadas a 0.0");
  }

  return true;
}

// Función para enviar ubicación al servidor
void sendLocation(float lat, float lng, float speed) {
  // Crear JSON con la intensidad de señal actual
  DynamicJsonDocument doc(256);
  doc["device_id"] = deviceId;
  doc["lat"] = String(lat, 6);
  doc["lng"] = String(lng, 6);
  doc["speed"] = String(speed, 2);
  doc["vehicle_on"] = true;
  int signal_quality = 0;
  String csqResponse = sendATCommand("AT+CSQ", "+CSQ:", 100); // Obtener respuesta directamente
  Serial.println("Respuesta CSQ cruda: " + csqResponse); // Depuración
  int startIdx = csqResponse.indexOf("+CSQ:") + 5; // Saltar el prefijo "+CSQ:"
  int commaIdx = csqResponse.indexOf(',', startIdx);
  if (commaIdx != -1 && startIdx > 0 && commaIdx > startIdx) {
    String rssiStr = csqResponse.substring(startIdx, commaIdx);
    rssiStr.trim(); // Modificar directamente el String
    signal_quality = rssiStr.toInt();
    Serial.print("Valor raw de rssiStr: '"); Serial.print(rssiStr); Serial.println("'");
    if (signal_quality >= 0 && signal_quality <= 31) {
      Serial.println("Intensidad de señal actual: " + String(signal_quality));
    } else {
      Serial.println("Valor de señal inválido, usando 0");
      signal_quality = 0;
    }
  } else {
    Serial.println("No se pudo parsear la intensidad de señal, usando 0");
  }
  doc["signal_quality"] = signal_quality;
  String payload;
  serializeJson(doc, payload);
  Serial.println("Payload enviado: " + payload);

  // Inicializar HTTPS
  Serial.println("Inicializando HTTPS...");
  if (!modem.https_begin()) {
    Serial.println("Fallo al inicializar HTTPS");
    return;
  }

  // Configurar URL
  String url = "https://" + String(server) + String(resource);
  Serial.println("URL configurada: " + url); // Depuración de la URL
  if (!modem.https_set_url(url)) {
    Serial.println("Fallo al configurar la URL");
    modem.https_end();
    return;
  }

  // Agregar encabezados ajustados (mínimos como en Postman)
  modem.https_add_header("Content-Type", "application/json");
  modem.https_add_header("Content-Length", String(payload.length())); // Longitud explícita
  // Si Postman usa encabezados adicionales, añádelos aquí (comparte los detalles)

  // Enviar solicitud POST
  Serial.println("Enviando solicitud POST...");
  if (client.connect(server, 443)) {
    Serial.println("Conectado al servidor");
    client.println("POST " + String(resource) + " HTTP/1.1");
    client.println("Host: " + String(server));
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(payload.length()));
    client.println("Connection: close");
    client.println();
    client.println(payload);

    // Leer la respuesta completa con tiempos reducidos
    unsigned long timeout = millis();
    while (client.connected() && millis() - timeout < 2000) { // Timeout de 5 segundos
      while (client.available()) {
        String line = client.readStringUntil('\n');
        Serial.print("Respuesta del servidor: ");
        Serial.println(line);
      }
      if (millis() - timeout > 1000) break; // Salir si no hay más datos después de 2 segundos
    }
    client.stop();
  } else {
    Serial.println("Fallo al conectar al servidor");
  }

  // Desconectar (opcional, ya manejado por client.stop())
  modem.https_end();
}

// Función para enviar comandos AT y devolver la respuesta
String sendATCommand(String cmd, String expected, int timeout) {
  Serial.println("Enviando: " + cmd);
  SerialAT.println(cmd);
  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (SerialAT.available()) {
      response += (char)SerialAT.read();
    }
    if (response.indexOf(expected) != -1) {
      Serial.println("Respuesta AT: " + response);
      return response;
    }
    delay(10);
  }
  Serial.println("Respuesta AT (timeout): " + response);
  return response;
}

// Función para leer respuesta AT (no usada directamente ahora)
String readATResponse(int timeout) {
  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (SerialAT.available()) {
      response += (char)SerialAT.read();
    }
    delay(10);
  }
  return response;
}

// Función para esperar respuesta AT (usada internamente por sendATCommand)
bool waitForResponse(String expected, int timeout) {
  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (SerialAT.available()) {
      response += (char)SerialAT.read();
    }
    if (response.indexOf(expected) != -1) {
      Serial.println("Respuesta AT: " + response);
      return true;
    }
    delay(10);
  }
  Serial.println("Respuesta AT (timeout): " + response);
  return false;
}