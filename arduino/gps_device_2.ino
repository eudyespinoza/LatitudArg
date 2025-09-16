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
  Serial.println("Iniciando prueba de modem... [Hora: 5:23 PM -03, 18/07/2025]");

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
      Serial.println("Datos GPS parseados: Lat=" + String(lat, 6) + ", Lng=" + String(lng, 6) + ", Speed=" + String(speed, 2) + " km/h");
      sendLocation(lat, lng, speed);
    } else {
      Serial.println("No se pudieron parsear las coordenadas GPS");
    }
  } else {
    Serial.println("No se obtuvieron datos GPS");
  }
  delay(10000); // Actualizar cada 10 segundos
}

// Función para obtener datos GPS desde Serial2
String getGPSData() {
  String response = "";
  String rmcData = "";
  unsigned long start = millis();
  // Limpiar buffer antes de leer
  while (SerialGPS.available()) SerialGPS.read();
  while (millis() - start < 3000) {
    while (SerialGPS.available()) {
      char c = SerialGPS.read();
      Serial.write(c); // Mostrar datos crudos para depuración
      response += c;
      if (response.endsWith("\r\n")) {
        if (response.startsWith("$GPRMC")) {
          response.trim(); // Eliminar espacios o caracteres finales
          // Verificar si tiene fix válido y el formato básico
          int firstComma = response.indexOf(',');
          int secondComma = response.indexOf(',', firstComma + 1);
          int thirdComma = response.indexOf(',', secondComma + 1);
          if (thirdComma != -1 && response.substring(secondComma + 1, thirdComma) == "A") {
            rmcData = response;
            break; // Salir del bucle interno para evitar concatenar más sentencias
          }
        }
        response = ""; // Reiniciar para la siguiente sentencia
      }
    }
    if (rmcData != "") break; // Salir del bucle externo si se capturó una sentencia válida
    delay(10);
  }
  if (rmcData == "") {
    Serial.println("No se encontró sentencia $GPRMC válida");
  } else {
    Serial.println("Sentencia $GPRMC capturada: " + rmcData);
  }
  return rmcData;
}

// Función para parsear datos GPS
bool parseGPSData(String gpsData, float &lat, float &lng, float &speed) {
  if (!gpsData.startsWith("$GPRMC")) {
    Serial.println("No se recibió sentencia $GPRMC");
    return false;
  }

  // Verificar que sea una sola sentencia (no debe contener múltiples \r\n)
  if (gpsData.indexOf("\r\n", gpsData.indexOf("\r\n") + 1) != -1) {
    Serial.println("Error: Múltiples sentencias en gpsData: " + gpsData);
    return false;
  }

  // Parsear la sentencia $GPRMC
  int commas[13]; // Aumentado a 13 para cubrir todos los campos hasta el checksum
  commas[0] = -1;
  int commaCount = 0;
  for (int i = 1; i < 13; i++) {
    commas[i] = gpsData.indexOf(',', commas[i - 1] + 1);
    if (commas[i] == -1) break;
    commaCount++;
  }
  if (commaCount < 7) { // Necesitamos al menos 8 campos (hasta velocidad)
    Serial.println("Formato de sentencia $GPRMC inválido, comas encontradas: " + String(commaCount));
    return false;
  }

  // Depuración: Imprimir posiciones de las comas
  Serial.print("Posiciones de comas: ");
  for (int i = 0; i <= commaCount; i++) {
    Serial.print(commas[i]);
    Serial.print(" ");
  }
  Serial.println();

  // Verificar estado (A = válido, V = inválido)
  String status = gpsData.substring(commas[2] + 1, commas[3]);
  status.trim();
  Serial.println("Campo de estado extraído: '" + status + "'");
  if (status != "A") {
    Serial.println("Sin fix válido (status = " + status + ")");
    lat = 0.0;
    lng = 0.0;
    speed = 0.0;
    return false;
  }

  // Latitud
  String latStr = gpsData.substring(commas[3] + 1, commas[4]);
  String latDir = gpsData.substring(commas[4] + 1, commas[5]);
  latStr.trim();
  latDir.trim();
  if (latStr != "" && latStr.length() >= 4 && latStr.indexOf('.') != -1) {
    float latDeg = latStr.substring(0, 2).toFloat();
    float latMin = latStr.substring(2).toFloat();
    lat = latDeg + (latMin / 60.0);
    if (latDir == "S") lat = -lat;
  } else {
    lat = 0.0;
    Serial.println("Sin datos de latitud válidos: " + latStr);
  }

  // Longitud
  String lngStr = gpsData.substring(commas[5] + 1, commas[6]);
  String lngDir = gpsData.substring(commas[6] + 1, commas[7]);
  lngStr.trim();
  lngDir.trim();
  if (lngStr != "" && lngStr.length() >= 5 && lngStr.indexOf('.') != -1) {
    float lngDeg = lngStr.substring(0, 3).toFloat();
    float lngMin = lngStr.substring(3).toFloat();
    lng = lngDeg + (lngMin / 60.0);
    if (lngDir == "W") lng = -lng;
  } else {
    lng = 0.0;
    Serial.println("Sin datos de longitud válidos: " + lngStr);
  }

  // Velocidad
  String speedStr = gpsData.substring(commas[7] + 1, commas[8]);
  speedStr.trim();
  if (speedStr != "" && speedStr.indexOf('.') != -1) {
    speed = speedStr.toFloat() * 1.852; // Convertir nudos a km/h
    Serial.println("Velocidad detectada: " + String(speed, 2) + " km/h");
  } else {
    speed = 0.0;
    Serial.println("Sin velocidad válida: " + speedStr);
  }

  // Validar que latitud y longitud sean válidas
  bool valid = (lat != 0.0 && lng != 0.0);
  if (!valid) {
    Serial.println("Datos GPS no válidos: Lat=" + String(lat, 6) + ", Lng=" + String(lng, 6));
  }
  return valid;
}

// Función para enviar ubicación al servidor
void sendLocation(float lat, float lng, float speed) {
  // Limpiar buffers seriales para evitar interferencias
  while (SerialGPS.available()) SerialGPS.read();
  while (SerialAT.available()) SerialAT.read();

  // Crear JSON con la intensidad de señal actual
  DynamicJsonDocument doc(256);
  doc["device_id"] = deviceId;
  doc["lat"] = String(lat, 6);
  doc["lng"] = String(lng, 6);
  doc["speed"] = String(speed, 2);
  doc["vehicle_on"] = true;
  int signal_quality = 0;
  unsigned long startTime = millis();
  String csqResponse = sendATCommand("AT+CSQ", "+CSQ:", 100); // Obtener respuesta directamente
  Serial.println("Tiempo AT+CSQ: " + String(millis() - startTime) + " ms");
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
  startTime = millis();
  if (!modem.https_begin()) {
    Serial.println("Fallo al inicializar HTTPS");
    Serial.println("Tiempo HTTPS init: " + String(millis() - startTime) + " ms");
    return;
  }
  Serial.println("Tiempo HTTPS init: " + String(millis() - startTime) + " ms");

  // Configurar URL
  String url = "https://" + String(server) + String(resource);
  Serial.println("URL configurada: " + url); // Depuración de la URL
  startTime = millis();
  if (!modem.https_set_url(url)) {
    Serial.println("Fallo al configurar la URL");
    Serial.println("Tiempo URL config: " + String(millis() - startTime) + " ms");
    modem.https_end();
    return;
  }
  Serial.println("Tiempo URL config: " + String(millis() - startTime) + " ms");

  // Configurar timeout del cliente
  client.setTimeout(10000); // Timeout de 10 segundos

  // Enviar solicitud POST
  Serial.println("Enviando solicitud POST...");
  startTime = millis();
  if (client.connect(server, 443)) {
    Serial.println("Conectado al servidor");
    Serial.println("Tiempo conexión: " + String(millis() - startTime) + " ms");

    // Enviar la solicitud POST completa (idéntica al código original)
    startTime = millis();
    client.println("POST " + String(resource) + " HTTP/1.1");
    client.println("Host: " + String(server));
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(payload.length()));
    client.println("Connection: close");
    client.println();
    client.println(payload);
    Serial.println("Tiempo envío POST: " + String(millis() - startTime) + " ms");

    // Delay antes de leer el cuerpo
    delay(300); // Espera 300 ms para dar tiempo al servidor

    // Leer la respuesta completa
    String response = "";
    int bytesReceived = 0;
    startTime = millis();
    while (client.connected() || client.available()) {
      if (client.available()) {
        char buffer[128];
        int bytesRead = client.readBytes(buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
          buffer[bytesRead] = '\0';
          response += buffer;
          bytesReceived += bytesRead;
          Serial.println("Datos recibidos (" + String(bytesRead) + " bytes): " + String(buffer));
        }
      }
      delay(10); // Pequeña pausa para evitar saturar la CPU
      if (millis() - startTime > 15000) { // Timeout de 15 segundos
        Serial.println("Timeout de lectura alcanzado");
        break;
      }
    }
    Serial.println("Tiempo lectura respuesta: " + String(millis() - startTime) + " ms");
    Serial.println("Total bytes recibidos: " + String(bytesReceived));

    // Mostrar la respuesta completa
    if (response != "") {
      Serial.println("Respuesta completa del servidor:");
      Serial.println(response);
    } else {
      Serial.println("No se recibió respuesta del servidor");
    }

    client.stop();
    Serial.println("Cliente desconectado");
  } else {
    Serial.println("Fallo al conectar al servidor");
    Serial.println("Tiempo conexión fallida: " + String(millis() - startTime) + " ms");
  }

  // Desconectar
  startTime = millis();
  modem.https_end();
  Serial.println("Tiempo HTTPS end: " + String(millis() - startTime) + " ms");
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