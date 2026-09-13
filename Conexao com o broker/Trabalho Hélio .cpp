#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// --- ATRIBUIÇÃO DE PINOS (APENAS LADO DIREITO) ---
#define DHTPIN     32   // Sinal do DHT11 (GPIO 32)
#define DHTTYPE    DHT11
#define SDA_PIN    33   // I2C SDA (GPIO 33)
#define SCL_PIN    25   // I2C SCL (GPIO 25)
#define RELE_PIN   26   // Controle do Relé (GPIO 26)

// Instancia o DHT11 e o LCD
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x3F, 16, 2);

// --- REDE E MQTT ---
const char* ssid = "Bar do pedro";
const char* password = "03031968";
const char* mqtt_server = "192.168.1.109";
const int mqtt_port = 1883;

// --- TÓPICOS MQTT ---
const char* topic_temp = "casa/sala/temperatura";
const char* topic_comando = "casa/sala/ventilador/comando";
const char* topic_status = "casa/sala/ventilador/status";

WiFiClient espClient;
PubSubClient client(espClient);

// --- VARIÁVEIS DE CONTROLE ---
unsigned long lastMsg = 0;
float temperaturaAtual = 0.0;
bool modoManual = false;       // Se true, ignora o controle automático por temperatura
bool estadoVentilador = false; // Estado atual do relé

// Atualiza o Display LCD I2C
void atualizarLCD() {
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    if (isnan(temperaturaAtual)) {
        lcd.print("ERRO   ");
    } else {
        lcd.print(temperaturaAtual, 1);
        lcd.print((char)223); // Símbolo de grau (°)
        lcd.print("C   ");
    }

    lcd.setCursor(0, 1);
    lcd.print("Fan: ");
    lcd.print(estadoVentilador ? "LIGADO " : "DESLIG ");
    lcd.print(modoManual ? "[M]" : "[A]");
}

// Aciona o Relé e envia o status para o MQTT
void acionarVentilador(bool ligar) {
    if (estadoVentilador != ligar) {
        estadoVentilador = ligar;
        
        // Acionamento digital do relé
        // (Altere para LOW se o seu módulo de relé for Ativo em Nível Baixo)
        digitalWrite(RELE_PIN, estadoVentilador ? HIGH : LOW);
        
        // Publica o novo status no MQTT
        String statusMsg = estadoVentilador ? "LIGADO" : "DESLIGADO";
        client.publish(topic_status, statusMsg.c_str());
        Serial.print("-> Status do Ventilador alterado: ");
        Serial.println(statusMsg);
    }
    atualizarLCD();
}

// Callback: Trata as mensagens recebidas via MQTT
void callback(char* topic, byte* payload, unsigned int length) {
    String mensagem = "";
    for (unsigned int i = 0; i < length; i++) {
        mensagem += (char)payload[i];
    }
    mensagem.trim();
    String topicoStr = String(topic);

    Serial.print("Mensagem recebida [");
    Serial.print(topicoStr);
    Serial.print("]: ");
    Serial.println(mensagem);

    // Processa comandos manuais de sobrescrita
    if (topicoStr == topic_comando) {
        if (mensagem == "LIGAR" || mensagem == "1") {
            modoManual = true;
            acionarVentilador(true);
            Serial.println("-> Comando recebido: Sobrescrita MANUAL (LIGAR)");
        } else if (mensagem == "DESLIGAR" || mensagem == "0") {
            modoManual = true;
            acionarVentilador(false);
            Serial.println("-> Comando recebido: Sobrescrita MANUAL (DESLIGAR)");
        } else if (mensagem == "AUTO") {
            modoManual = false;
            Serial.println("-> Modo retornado para AUTOMATICO");
            if (temperaturaAtual >= 30.0) acionarVentilador(true);
            else if (temperaturaAtual <= 25.0) acionarVentilador(false);
        }
    }
}

void Connect_to_WiFi() {
    int i = 0;
    Serial.println("Connecting to WiFi ...");
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED && i <= 20) {
        delay(500);
        Serial.print(".");
        i++;
    }
    Serial.println("");

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi!");
        Serial.print("IP da ESP32: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Could not connect to the WiFi network");
    }
}

void reconnect_MQTT() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        String clientId = "ESP32-ProjetoGeral-";
        clientId += String(random(0xffff), HEX);

        if (client.connect(clientId.c_str())) {
            Serial.println("connected to MQTT Broker!");
            client.subscribe(topic_comando);
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void setup() {
    pinMode(RELE_PIN, OUTPUT);
    digitalWrite(RELE_PIN, LOW); // Inicia relé desligado

    Serial.begin(115200);

    // Inicializa o barramento I2C nos GPIOs 33 (SDA) e 25 (SCL)
    Wire.begin(SDA_PIN, SCL_PIN);
    
    // Inicializa o Sensor DHT11
    dht.begin();

    // Inicializa o Display LCD I2C
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Iniciando...");

    Connect_to_WiFi();

    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

     lcd.print("Sistema inicializado ");
     delay (5000);
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) Connect_to_WiFi();
    if (!client.connected()) reconnect_MQTT();
    client.loop();

    unsigned long now = millis();
    // Leitura real do DHT11 a cada 3 segundos
    if (now - lastMsg > 3000) {
        lastMsg = now;

        float tempLida = dht.readTemperature();

        if (!isnan(tempLida)) {
            temperaturaAtual = tempLida;

            // Publica a leitura real no MQTT
            String payload = String(temperaturaAtual, 1);
            client.publish(topic_temp, payload.c_str());
            
            Serial.print("Temperatura lida e publicada: ");
            Serial.print(payload);
            Serial.println(" °C");

            // Lógica de acionamento automático por histerese
            if (!modoManual) {
                if (temperaturaAtual >= 30.0) {
                    acionarVentilador(true);  // Liga em >= 30°C
                } else if (temperaturaAtual <= 25.0) {
                    acionarVentilador(false); // Desliga em <= 25°C
                }
            }
            atualizarLCD();
        } else {
            Serial.println("Falha ao ler o sensor DHT11!");
        }
    }
}
