#include <Preferences.h>
#include <BluetoothSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>

Preferences preferences;
BluetoothSerial SerialBT;
HTTPCliente http;
String ssid = "";
String password = "";
String uuid = "";
volatile double waterFlow;

void setup() {
  Serial.begin(115200);

  waterFlow = 0;
  // Iniciar Bluetooth
  SerialBT.begin("WaterWise");  // Nombre del dispositivo Bluetooth


  attachInterrupt(34, pulse, RISING);

  // Iniciar NVS para almacenar las credenciales
  preferences.begin("wifiCreds", false);

  // Intentar obtener credenciales WiFi desde NVS
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  uuid = preferences.getString("uuid", "");

  if (ssid == "" || password == "" || uuid == "") {
    Serial.println("No se encontraron credenciales WiFi y/o UUID. Esperando información vía Bluetooth...");
  } else {
    Serial.println("Credenciales WiFi encontradas y UUID.");
    Serial.println("UUID" + uuid);
    connectToWiFi(ssid, password);
  }
}

void loop() {
  // Si no hay credenciales WiFi almacenadas, escuchar el Bluetooth
  if (ssid == "" || password == "" || uuid == "") {
    if (SerialBT.available()) {
      SerialBT.printf("SSID:[NOMBRE RED WIFI]\nPASS:[CONTRASEÑA]\nUUID:[UUID]\n");
      String incomingData = SerialBT.readStringUntil('\n');
      incomingData.trim();  // Eliminar espacios en blanco

      if (incomingData.startsWith("SSID:")) {
        ssid = incomingData.substring(5);  // Obtener SSID después de "SSID:"
        Serial.println("SSID recibido: " + ssid);
      } else if (incomingData.startsWith("PASS:")) {
        password = incomingData.substring(5);  // Obtener contraseña después de "PASS:"
        Serial.println("Contraseña recibida: " + password);
      } else if (incomingData.startsWith("UUID:")) {
        uuid = incomingData.substring(5);  // Obtener UUID después de "UUID:"
        Serial.println("UUID recibido: " + uuid);
      }
      // Si ya se tienen ambas credenciales, guardarlas en NVS y conectar
      if (ssid != "" && password != "" && uuid != "") {
        Serial.println("Guardando credenciales y UUID en NVS...");
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.putString("uuid", uuid);
        connectToWiFi(ssid, password);
      }
    }
  } else {

    http.begin("https://waterwise.com.co/api/v1/measures");
    http.addHeader("Content-Type", "application/json");
    String postData = "{\"device_id\": \"" + uuid + "\", \"value\": " + String(waterFlow) + ", \"in_time\": \"" + String(5) + "\"}";
    int httpResponseCode = http.POST(postData);
    Serial.printf("Flujo de agua actual: %.2f L\n", waterFlow);

    if (httpResponseCode > 0) {
      String response = http.getString();  // Obtiene la respuesta del servidor
      Serial.println(httpResponseCode);    // Código de respuesta
      Serial.println(response);            // Cuerpo de la respuesta
    } else {
      Serial.print("Error en la solicitud: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  }
  delay(5000);
}

void pulse() {
  waterFlow += 1.0 / 450.0;
}

void connectToWiFi(String ssid, String password) {
  Serial.println("Conectando a WiFi...");
  WiFi.begin(ssid.c_str(), password.c_str());

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConexión exitosa!");
    Serial.println("Dirección IP: ");
    Serial.println(WiFi.localIP());
    Serial.println("UUID: " + uuid);
  } else {
    Serial.println("\nNo se pudo conectar a la red WiFi.");
    preferences.clear();
  }

  preferences.end();  // Terminar el uso de NVS
}
