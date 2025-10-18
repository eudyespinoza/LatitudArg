// ==========================
// Firmware robusto LILYGO T-A7670
// ==========================
#define LILYGO_T_A7670

#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include "utilities.h"                 // Debe definir: MODEM_BAUDRATE, MODEM_RX_PIN, MODEM_TX_PIN, BOARD_PWRKEY_PIN, BOARD_POWERON_PIN, (opcional) MODEM_RESET_PIN, MODEM_RESET_LEVEL
#include "TinyGsmClientA76xxSSL.h"
#include <esp_task_wdt.h>              // Watchdog ESP32

// UARTs
#define SerialAT Serial1   // UART hacia el módem
#define SerialGPS Serial2  // UART del GPS integrado (IO19 RX, IO23 TX)

// Configuración (NO tocar si no se desea)
const char apn[]      = "wap.gprs.unifon.com.ar";
const char gprsUser[] = "wap";
const char gprsPass[] = "wap";
const char simPIN[]   = "1234";
const char server[]   = "latitudarg.com";
const char resource[] = "/api/update_location";
const char deviceId[] = "672-AE766";

// Objetos modem/cliente SSL
TinyGsmA76xxSSL modem(SerialAT);
TinyGsmA76xxSSL::GsmClientSecureA76xxSSL client(modem);

// Parámetros de tiempo y reintentos
const unsigned long SEND_INTERVAL_MS = 10000UL;       // Envío periódico (10s)
const unsigned long GPS_READ_TIMEOUT_MS = 3000UL;     // Timeout lectura GPS
const unsigned long HTTPS_READ_TIMEOUT_MS = 10000UL;  // Timeout lectura HTTPS (reducido a 10s)
const int MAX_MODEM_INIT_TRIES = 5;
const int MAX_GPRS_RETRIES = 3;
const int MAX_POST_RETRIES = 2;
const int AT_COMMAND_TIMEOUT_MS = 2000;               // Timeout para comandos AT (2s)

// Watchdog: aumentado a 60s para dar más inactividad antes de reinicios
const int WDT_TIMEOUT_SEC = 60;                       // Watchdog (segundos)

// Estado global
unsigned long lastSendTimestamp = 0;
bool gprsConnected = false;

// Prototipos
String sendATCommand(const String &cmd, const String &expected, int timeout);
String readATAll(int timeout);
String getGPSData();
bool parseGPSData(const String &gpsData, float &lat, float &lng, float &speed);
bool ensureModemInitialized();
bool connectGPRS();
void resetModemHardware();
bool sendLocationWithRetries(float lat, float lng, float speed, int maxRetries);
bool sendLocationOnce(float lat, float lng, float speed, bool &shutdownRequest, bool &transmitAudioRequest);
void feedWatchdog();
void modemTask(void *pvParameters);

void setup() {
  Serial.begin(115200);
  unsigned long end = millis() + 10;
  while (millis() < end) { feedWatchdog(); delay(10); }
  Serial.println();
  Serial.println("=== Iniciando sistema robusto LILYGO T-A7670 ===");

  // Asegurar inicialización segura del TWDT:
  // Si existe uno previo, deinicializar y reconfigurar con mayor timeout.
  esp_err_t derr = esp_task_wdt_deinit(); // puede devolver error si no estaba inicializado
  if (derr == ESP_OK) {
    Serial.println("TWDT deinitialized (había uno previo).");
  } else {
    Serial.printf("TWDT deinit returned: %d (continuando)\n", derr);
  }

  esp_task_wdt_config_t twdt_config = {
    .timeout_ms = (uint32_t)WDT_TIMEOUT_SEC * 1000U,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, // monitor all cores
    .trigger_panic = true
  };
  esp_err_t init_ret = esp_task_wdt_init(&twdt_config);
  if (init_ret != ESP_OK) {
    Serial.printf("Error inicializando TWDT: %d\n", init_ret);
  } else {
    Serial.printf("TWDT inicializado con timeout %d s\n", WDT_TIMEOUT_SEC);
  }

  // Registrar la tarea principal (loopTask) en el TWDT existente
  esp_err_t ret = esp_task_wdt_add(NULL);
  if (ret != ESP_OK) {
    Serial.printf("Error al registrar loopTask en TWDT: %d\n", ret);
  } else {
    Serial.println("loopTask registrado en TWDT.");
  }

  // Crear tarea para manejar el módem y el envío
  BaseType_t xt = xTaskCreate(modemTask, "ModemTask", 8192, NULL, 1, NULL);
  if (xt != pdPASS) {
    Serial.println("Error al crear ModemTask!");
  } else {
    Serial.println("ModemTask creada.");
  }

  Serial.println("Setup completado. Entrando a loop principal.");
}

void loop() {
  feedWatchdog(); // Alimentar watchdog en el loop principal
  delay(50);      // Pequeña pausa para liberar CPU
}

/* ================= Tarea para manejar el módem ================= */
void modemTask(void *pvParameters) {
  // Registrar esta tarea en el TWDT
  esp_err_t ret = esp_task_wdt_add(xTaskGetCurrentTaskHandle());
  if (ret != ESP_OK) {
    Serial.printf("Error al registrar modemTask en TWDT: %d\n", ret);
  } else {
    Serial.println("modemTask registrado en TWDT.");
  }

  // Configurar pines del módem
  pinMode(BOARD_PWRKEY_PIN, OUTPUT);
  pinMode(BOARD_POWERON_PIN, OUTPUT);
#ifdef MODEM_RESET_PIN
  pinMode(MODEM_RESET_PIN, OUTPUT);
#endif

  // Secuencia de reset hardware (si existe)
#ifdef MODEM_RESET_PIN
  Serial.println("Modem reseteado por pin MODEM_RESET_PIN");
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
  unsigned long endReset = millis() + 100;
  while (millis() < endReset) { feedWatchdog(); delay(10); }
  digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL);
  endReset = millis() + 2600;
  while (millis() < endReset) { feedWatchdog(); delay(10); }
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
#endif

  // Encendido básico (PWRKEY)
  Serial.println("Encendiendo modem (PWRKEY)...");
  digitalWrite(BOARD_POWERON_PIN, HIGH);
  digitalWrite(BOARD_PWRKEY_PIN, LOW);
  unsigned long endPwr = millis() + 100;
  while (millis() < endPwr) { feedWatchdog(); delay(10); }
  digitalWrite(BOARD_PWRKEY_PIN, HIGH);
  endPwr = millis() + 120;
  while (millis() < endPwr) { feedWatchdog(); delay(10); }
  digitalWrite(BOARD_PWRKEY_PIN, LOW);
  endPwr = millis() + 3000;
  while (millis() < endPwr) { feedWatchdog(); delay(10); }

  // Iniciar Seriales
  SerialAT.begin(MODEM_BAUDRATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  SerialGPS.begin(9600, SERIAL_8N1, 19, 23); // RX=19, TX=23 para GPS integrado
  unsigned long endSerial = millis() + 500;
  while (millis() < endSerial) { feedWatchdog(); delay(10); }

  // Probar comunicación básica con el módem
  String atResp = sendATCommand("AT", "OK", AT_COMMAND_TIMEOUT_MS);
  Serial.println("Respuesta AT básica: " + atResp);

  // Inicializar módem con reintentos
  if (!ensureModemInitialized()) {
    Serial.println("ERROR: No se pudo inicializar el módem tras varios intentos. Reiniciando...");
    unsigned long endErr = millis() + 200;
    while (millis() < endErr) { feedWatchdog(); delay(10); }
    ESP.restart();
  }

  // Desbloquear SIM si PIN presente
  if (simPIN[0] != '\0') {
    String resp = sendATCommand("AT+CPIN=\"" + String(simPIN) + "\"", "OK", AT_COMMAND_TIMEOUT_MS);
    if (resp.length() == 0) {
      Serial.println("Advertencia: AT+CPIN no devolvió OK (continuando)");
    }
  }

  // Verificar SIM lista
  bool simReady = false;
  for (int i = 0; i < 5; ++i) {
    String res = sendATCommand("AT+CPIN?", "+CPIN:", AT_COMMAND_TIMEOUT_MS);
    if (res.indexOf("READY") != -1) {
      simReady = true;
      Serial.println("SIM lista");
      break;
    }
    Serial.println("SIM no lista aun, reintentando...");
    unsigned long endSim = millis() + 1000;
    while (millis() < endSim) { feedWatchdog(); delay(10); }
  }
  if (!simReady) {
    Serial.println("ERROR: SIM no lista tras reintentos. Reiniciando...");
    unsigned long endErr = millis() + 200;
    while (millis() < endErr) { feedWatchdog(); delay(10); }
    ESP.restart();
  }

  // Configurar contexto PDP (APN)
  if (sendATCommand("AT+CGDCONT=1,\"IP\",\"" + String(apn) + "\"", "OK", AT_COMMAND_TIMEOUT_MS).length() == 0) {
    Serial.println("Advertencia: fallo al configurar CGDCONT (se continúa)");
  }

  while (1) {
    feedWatchdog(); // Alimentar watchdog en la tarea

    // Si perdimos GPRS, reintentar (no bloquear)
    if (!gprsConnected) {
      Serial.println("GPRS no conectado, reintentando conexión...");
      gprsConnected = connectGPRS();
      if (!gprsConnected) {
        unsigned long waitUntil = millis() + 2000;
        while (millis() < waitUntil) {
          feedWatchdog();
          delay(50);
        }
        continue;
      }
    }

    // Envío periódico controlado por millis()
    if (millis() - lastSendTimestamp >= SEND_INTERVAL_MS) {
      lastSendTimestamp = millis();

      // Obtener sentencia $GPRMC válida del GPS
      String rmc = getGPSData();
      float lat = 0.0, lng = 0.0, speed = 0.0;
      if (rmc.length() == 0) {
        Serial.println("No se obtuvo sentencia $GPRMC válida (timeout). Enviando lat=0, lng=0, speed=0.");
      } else if (!parseGPSData(rmc, lat, lng, speed)) {
        Serial.println("Error parseando GPS. Enviando lat=0, lng=0, speed=0.");
        lat = 0.0;
        lng = 0.0;
        speed = 0.0;
      } else {
        Serial.println("Datos GPS válidos obtenidos.");
      }

      // Intentar enviar con reintentos manejados, incluso con lat=0, lng=0
      if (!sendLocationWithRetries(lat, lng, speed, MAX_POST_RETRIES)) {
        Serial.println("Fallo envío tras reintentos. Intentando reconectar GPRS y/o reinicializar modem si es necesario.");
        gprsConnected = false;
      }
    }

    // Pequeña pausa para liberar CPU (sin bloquear mucho)
    vTaskDelay(50 / portTICK_PERIOD_MS);
    feedWatchdog();
  }
}

/* ================= Funciones auxiliares ================= */

// Envía un comando AT al módem y espera que aparezca "expected" en la respuesta (timeout en ms)
String sendATCommand(const String &cmd, const String &expected, int timeout) {
  Serial.print("AT -> ");
  Serial.println(cmd);
  while (SerialAT.available()) SerialAT.read();
  SerialAT.println(cmd);
  unsigned long start = millis();
  String response;
  while (millis() - start < (unsigned long)timeout) {
    while (SerialAT.available()) {
      char c = (char)SerialAT.read();
      response += c;
    }
    if (expected.length() && response.indexOf(expected) != -1) {
      Serial.print("AT <- ");
      Serial.println(response);
      return response;
    }
    delay(5);
    feedWatchdog(); // Alimentar watchdog durante espera
  }
  if (response.length()) {
    Serial.print("AT timeout, respuesta parcial: ");
    Serial.println(response);
  } else {
    Serial.println("AT timeout sin respuesta.");
  }
  if (expected.length()) {
    return String();
  }
  return response;
}

// Lee todo lo disponible del SerialAT por hasta timeout ms
String readATAll(int timeout) {
  unsigned long start = millis();
  String r;
  while (millis() - start < (unsigned long)timeout) {
    while (SerialAT.available()) {
      r += (char)SerialAT.read();
    }
    delay(5);
    feedWatchdog(); // Alimentar watchdog durante espera
  }
  return r;
}

// Inicializa modem con reintentos y secuencia de PWRKEY si es necesario
bool ensureModemInitialized() {
  for (int attempt = 1; attempt <= MAX_MODEM_INIT_TRIES; ++attempt) {
    Serial.printf("Intento inicializar modem (%d/%d)\n", attempt, MAX_MODEM_INIT_TRIES);
    unsigned long start = millis();
    if (modem.init()) {
      Serial.printf("Modem inicializado correctamente en %lums.\n", millis() - start);
      return true;
    }
    Serial.printf("modem.init() falló en %lums. Intentando secuencia PWRKEY/reset...\n", millis() - start);
    resetModemHardware();
    // espera breve (alimentar watchdog en espera)
    unsigned long end = millis() + 2000;
    while (millis() < end) {
      feedWatchdog();
      delay(10);
    }
    feedWatchdog(); // Alimentar después de cada intento
  }
  return false;
}

// Pulsar PWRKEY o reset hardware para tratar de recuperar módem
void resetModemHardware() {
#ifdef MODEM_RESET_PIN
  // Reset hardware si está disponible
  Serial.println("Aplicando pulso de reset hardware (MODEM_RESET_PIN)...");
  digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL);
  unsigned long endReset = millis() + 200;
  while (millis() < endReset) { feedWatchdog(); delay(10); }
  digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
  endReset = millis() + 1000;
  while (millis() < endReset) { feedWatchdog(); delay(10); }
#else
  // Pulsar PWRKEY como alternativa
  Serial.println("No hay MODEM_RESET_PIN; aplicando pulso PWRKEY...");
  digitalWrite(BOARD_PWRKEY_PIN, HIGH);
  unsigned long endPwr = millis() + 120;
  while (millis() < endPwr) { feedWatchdog(); delay(10); }
  digitalWrite(BOARD_PWRKEY_PIN, LOW);
  endPwr = millis() + 1000;
  while (millis() < endPwr) { feedWatchdog(); delay(10); }
#endif
}

// Intentar conectar GPRS con reintentos limitados
bool connectGPRS() {
  for (int attempt = 1; attempt <= MAX_GPRS_RETRIES; ++attempt) {
    Serial.printf("Intentando conectar GPRS (%d/%d)\n", attempt, MAX_GPRS_RETRIES);
    unsigned long start = millis();
    if (modem.gprsConnect(apn, gprsUser, gprsPass)) {
      Serial.printf("GPRS conectado en %lums.\n", millis() - start);
      return true;
    }
    Serial.printf("Fallo en gprsConnect() en %lums. Esperando y reintentando...\n", millis() - start);
    unsigned long end = millis() + 2000;
    while (millis() < end) {
      feedWatchdog();
      delay(50);
    }
    feedWatchdog(); // Alimentar después de cada intento
  }
  Serial.println("No se pudo establecer GPRS tras reintentos.");
  return false;
}

// Lee líneas de GPS hasta encontrar $GPRMC con status 'A' (válido) o timeout
String getGPSData() {
  unsigned long start = millis();
  String line;
  // Vaciar buffer viejo
  while (SerialGPS.available()) SerialGPS.read();

  while (millis() - start < GPS_READ_TIMEOUT_MS) {
    if (SerialGPS.available()) {
      line = SerialGPS.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;
      Serial.print("GPS crudo: ");
      Serial.println(line);
      if (line.startsWith("$GPRMC")) {
        // Chequeo de campo status (A=ok, V=void): está en la 3ª posición separada por comas
        // Ejemplo: $GPRMC,hhmmss.sss,A,lat,...
        int firstComma = line.indexOf(',');
        int secondComma = line.indexOf(',', firstComma + 1);
        int thirdComma = line.indexOf(',', secondComma + 1);
        if (secondComma != -1 && thirdComma != -1) {
          String status = line.substring(secondComma + 1, thirdComma);
          status.trim();
          Serial.print("GPS status detectado: '");
          Serial.print(status);
          Serial.println("'");
          if (status == "A") {
            // devolver la sentencia completa
            return line;
          } else {
            Serial.println("GPRMC sin fix (status != A). Seguir leyendo...");
          }
        }
      }
    }
    feedWatchdog();
    delay(10);
  }
  return String(); // vacío = timeout / no válido
}

// Parseo robusto de $GPRMC -> lat (deg), lng (deg), speed (km/h)
bool parseGPSData(const String &gpsData, float &lat, float &lng, float &speed) {
  if (!gpsData.startsWith("$GPRMC")) {
    Serial.println("parseGPSData: no es $GPRMC");
    return false;
  }

  // Separar por comas en un array (hasta 12 campos es suficiente)
  const int MAX_FIELDS = 12;
  String fields[MAX_FIELDS];
  int last = 0;
  int fieldIndex = 0;
  for (int i = 0; i < (int)gpsData.length() && fieldIndex < MAX_FIELDS; ++i) {
    if (gpsData[i] == ',') {
      fields[fieldIndex++] = gpsData.substring(last, i);
      last = i + 1;
    }
  }
  // último campo (resto)
  if (fieldIndex < MAX_FIELDS) {
    fields[fieldIndex++] = gpsData.substring(last);
  }

  // chequeos mínimos: necesitamos al menos hasta campo 7 (índice 7 = velocidad)
  // indices (según $GPRMC): 0=$GPRMC,1=time,2=status,3=lat,4=N/S,5=lon,6=E/W,7=sog,...
  if (fieldIndex < 8) {
    Serial.printf("parseGPSData: número de campos insuficiente (%d)\n", fieldIndex);
    return false;
  }
  String status = fields[2];
  status.trim();
  if (status != "A") {
    Serial.println("parseGPSData: status != A");
    return false;
  }

  String latStr = fields[3]; latStr.trim();
  String latDir = fields[4]; latDir.trim();
  String lngStr = fields[5]; lngStr.trim();
  String lngDir = fields[6]; lngDir.trim();
  String speedStr = fields[7]; speedStr.trim();

  if (latStr.length() < 4 || lngStr.length() < 5) {
    Serial.println("parseGPSData: lat/lng mal formateadas");
    return false;
  }

  // lat ddmm.mmmm -> grados decimales
  float latDeg = latStr.substring(0, 2).toFloat();
  float latMin = latStr.substring(2).toFloat();
  lat = latDeg + (latMin / 60.0);
  if (latDir == "S") lat = -lat;

  // lon dddmm.mmmm
  float lngDeg = lngStr.substring(0, 3).toFloat();
  float lngMin = lngStr.substring(3).toFloat();
  lng = lngDeg + (lngMin / 60.0);
  if (lngDir == "W") lng = -lng;

  // velocidad en nudos -> km/h
  speed = 0.0;
  if (speedStr.length() > 0) {
    // speedStr puede contener checksum si no se separó correctamente (ej: "0.00*hh")
    int star = speedStr.indexOf('*');
    String speedClean = (star == -1) ? speedStr : speedStr.substring(0, star);
    speed = speedClean.toFloat() * 1.852;
  }

  // Comprobación básica de validez
  if (lat == 0.0 && lng == 0.0) {
    Serial.println("parseGPSData: lat/lng resultaron 0, probablemente inválidos");
    return false;
  }

  Serial.printf("parseGPSData OK -> lat=%.6f lng=%.6f speed=%.2f km/h\n", lat, lng, speed);
  return true;
}

// Enviar ubicacion con varios reintentos (inteligente)
bool sendLocationWithRetries(float lat, float lng, float speed, int maxRetries) {
  for (int attempt = 1; attempt <= maxRetries; ++attempt) {
    Serial.printf("Enviar ubicacion intento %d/%d\n", attempt, maxRetries);
    if (!modem.isGprsConnected()) {
      Serial.println("GPRS no activo en el momento del envio. Intentando reconectar...");
      if (!connectGPRS()) {
        Serial.println("Reconexion GPRS fallida antes del envio.");
        if (attempt == maxRetries) {
          resetModemHardware();
        }
        gprsConnected = false;
        continue;
      }
      gprsConnected = true;
    }

    bool shutdownRequest = false;
    bool transmitAudioRequest = false;
    bool success = sendLocationOnce(lat, lng, speed, shutdownRequest, transmitAudioRequest);

    if (shutdownRequest || transmitAudioRequest) {
      Serial.printf("Comandos desde el servidor -> shutdown:%s transmit_audio:%s\n",
                    shutdownRequest ? "true" : "false",
                    transmitAudioRequest ? "true" : "false");
      // TODO: aplicar acciones de hardware segun corresponda.
    }

    if (success) {
      gprsConnected = true;
      int sq = modem.getSignalQuality();
      Serial.printf("Envio confirmado. SignalQuality=%d\n", sq);
      return true;
    }

    if (!modem.isGprsConnected()) {
      Serial.println("Post-send: GPRS perdio conexion despues del intento.");
      gprsConnected = false;
      if (attempt == maxRetries) {
        Serial.println("Ultimo intento fallido: aplicando reset hardware al modem.");
        resetModemHardware();
      }
    } else if (attempt == maxRetries) {
      Serial.println("Se agotaron los reintentos de envio sin respuesta exitosa.");
    }

    unsigned long waitUntil = millis() + 500;
    while (millis() < waitUntil) {
      feedWatchdog();
      delay(10);
    }
  }
  return false;
}

// Enviar una vez (envio HTTP POST sobre TLS, analiza codigo HTTP y muestra resultado)
bool sendLocationOnce(float lat, float lng, float speed, bool &shutdownRequest, bool &transmitAudioRequest) {
  shutdownRequest = false;
  transmitAudioRequest = false;

  DynamicJsonDocument doc(256);
  doc["device_id"] = deviceId;
  doc["lat"] = lat;
  doc["lng"] = lng;
  doc["speed"] = speed;
  doc["vehicle_on"] = true;
  int signal_quality = modem.getSignalQuality();
  if (signal_quality < 0) signal_quality = 0;
  doc["signal_quality"] = signal_quality;

  String payload;
  serializeJson(doc, payload);
  Serial.println("Payload JSON:");
  Serial.println(payload);

  client.setTimeout(HTTPS_READ_TIMEOUT_MS);
  unsigned long startConn = millis();
  if (!client.connect(server, 443)) {
    Serial.printf("Fallo al conectar a %s:443 (tiempo: %lums)\n", server, millis() - startConn);
    return false;
  }
  Serial.println("Conectado al servidor (TLS): enviando POST...");

  client.println("POST " + String(resource) + " HTTP/1.1");
  client.println("Host: " + String(server));
  client.println("User-Agent: LILYGO-T-A7670");
  client.println("Content-Type: application/json");
  client.println("Content-Length: " + String(payload.length()));
  client.println("Connection: close");
  client.println();
  client.print(payload);

  String response;
  unsigned long tStart = millis();
  while (millis() - tStart < HTTPS_READ_TIMEOUT_MS) {
    if (client.available()) {
      char buf[128];
      int r = client.readBytes(buf, sizeof(buf) - 1);
      if (r > 0) {
        buf[r] = '\0';
        response += String(buf);
        Serial.print(String(buf));
      }
    } else {
      delay(10);
    }
    feedWatchdog();
    if (!client.connected() && !client.available()) break;
  }

  client.stop();
  Serial.println();

  if (response.length() == 0) {
    Serial.println("No se recibio respuesta HTTP del servidor.");
    return false;
  }

  int firstCRLF = response.indexOf("\r\n");
  String statusLine = (firstCRLF == -1) ? response : response.substring(0, firstCRLF);
  Serial.print("Linea de estado HTTP: ");
  Serial.println(statusLine);

  int firstSpace = statusLine.indexOf(' ');
  int secondSpace = (firstSpace == -1) ? -1 : statusLine.indexOf(' ', firstSpace + 1);
  int httpCode = -1;
  if (firstSpace != -1 && secondSpace != -1) {
    String codeStr = statusLine.substring(firstSpace + 1, secondSpace);
    httpCode = codeStr.toInt();
  } else {
    int idx = statusLine.indexOf("HTTP/");
    if (idx != -1 && (int)statusLine.length() >= idx + 9) {
      String codeStr = statusLine.substring(idx + 9, idx + 12);
      httpCode = codeStr.toInt();
    }
  }

  Serial.printf("Codigo HTTP detectado: %d\n", httpCode);
  if (httpCode != 200) {
    Serial.printf("Envio NO exitoso. Codigo HTTP: %d. Respuesta completa (resumen):\n", httpCode);
    if (response.length() > 512) {
      Serial.println(response.substring(0, 512));
      Serial.println("... (respuesta truncada)");
    } else {
      Serial.println(response);
    }
    return false;
  }

  int bodyPos = response.indexOf("\r\n\r\n");
  if (bodyPos == -1 || bodyPos + 4 >= (int)response.length()) {
    Serial.println("No se encontro separacion cabeceras/cuerpo en la respuesta.");
    return false;
  }

  String body = response.substring(bodyPos + 4);
  body.trim();
  if (!body.length()) {
    Serial.println("Respuesta HTTP 200 pero sin cuerpo.");
    return false;
  }

  Serial.println("Cuerpo JSON recibido:");
  Serial.println(body);
  DynamicJsonDocument respDoc(1024);
  DeserializationError err = deserializeJson(respDoc, body);
  if (err) {
    Serial.println("Error parseando JSON respuesta: " + String(err.c_str()));
    return false;
  }

  serializeJsonPretty(respDoc, Serial);
  Serial.println();

  String status = respDoc["status"] | "";
  bool success = status.equalsIgnoreCase("success");
  if (!success) {
    Serial.println("API respondio con estado diferente de 'success'.");
  }

  if (respDoc.containsKey("shutdown")) {
    shutdownRequest = respDoc["shutdown"].as<bool>();
  }
  if (respDoc.containsKey("transmit_audio")) {
    transmitAudioRequest = respDoc["transmit_audio"].as<bool>();
  }

  if (success) {
    Serial.println("Envio exitoso (HTTP 200 + estado success).");
  }
  return success;
}

// Alimenta el watchdog (llamar con frecuencia)
void feedWatchdog() {
  esp_err_t ret = esp_task_wdt_reset();
  if (ret != ESP_OK) {
    Serial.printf("Error en esp_task_wdt_reset: %d\n", ret);
  }
}
