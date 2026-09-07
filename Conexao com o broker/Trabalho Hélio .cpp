#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- HARDWARE ---
int Motor = 25; // Pino do Motor/Ventilador (GPIO 25)
LiquidCrystal_I2C lcd(0x27, 16, 2);

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
bool modoManual = false;       // Se true, ignora o controle por temperatura
bool estadoVentilador = false; // Estado atual do motor

// Atualiza o Display LCD I2C
void atualizarLCD() {
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temperaturaAtual, 1);
    lcd.print((char)223); // Símbolo de grau (°)
    lcd.print("C   ");

    lcd.setCursor(0, 1);
    lcd.print("Fan: ");
    lcd.print(estadoVentilador ? "LIGADO " : "DESLIG ");
    lcd.print(modoManual ? "[M]" : "[A]");
}

// Controla o pino do motor e publica alteração de status
void acionarVentilador(bool ligar) {
    if (estadoVentilador != ligar) {
        estadoVentilador = ligar;
        
        // Liga em velocidade total ou desliga
        analogWrite(Motor, estadoVentilador ? 255 : 0);
        
        // Publica o novo status no MQTT
        String statusMsg = estadoVentilador ? "LIGADO" : "DESLIGADO";
        client.publish(topic_status, statusMsg.c_str());
        Serial.print("-> Status do Ventilador alterado: ");
        Serial.println(statusMsg);
    }
    atualizarLCD();
}

// Callback: Recebe tanto as temperaturas enviadas quanto os comandos manuais
void callback(char* topic, byte* payload, unsigned int length) {
    String mensagem = "";
    for (int i = 0; i < length; i++) {
        mensagem += (char)payload[i];
    }
    mensagem.trim();
    String topicoStr = String(topic);

    Serial.print("Mensagem recebida [");
    Serial.print(topicoStr);
    Serial.print("]: ");
    Serial.println(mensagem);

    // 1. Processa a Temperatura enviada (Seja pelo próprio loop ou externa)
    if (topicoStr == topic_temp) {
        temperaturaAtual = mensagem.toFloat();

        // Lógica automática (só executa se NÃO estiver em modo manual)
        if (!modoManual) {
            if (temperaturaAtual >= 30.0) {
                acionarVentilador(true);  // Liga se >= 30°C
            } else if (temperaturaAtual <= 25.0) {
                acionarVentilador(false); // Desliga se <= 25°C
            }
        }
        atualizarLCD();
    }

    // 2. Processa Comandos Manuais do MQTT Explorer
    if (topicoStr == topic_comando) {
        if (mensagem == "LIGAR" || mensagem == "1") {
            modoManual = true; // Entra no modo manual
            acionarVentilador(true);
            Serial.println("-> Comando recebido: Sobrescrita MANUAL (LIGAR)");
        } else if (mensagem == "DESLIGAR" || mensagem == "0") {
            modoManual = true; // Entra no modo manual
            acionarVentilador(false);
            Serial.println("-> Comando recebido: Sobrescrita MANUAL (DESLIGAR)");
        } else if (mensagem == "AUTO") {
            modoManual = false; // Retorna ao modo automático
            Serial.println("-> Modo retornado para AUTOMATICO");
            // Reavalia imediatamente a temperatura atual
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
            
            // Inscreve-se nos tópicos de temperatura e de comandos manuais
            client.subscribe(topic_temp);
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
    pinMode(Motor, OUTPUT);
    analogWrite(Motor, 0); // Inicia motor desligado

    Serial.begin(115200);

    // Inicialização do LCD I2C (SDA = GPIO 21, SCL = GPIO 22)
    Wire.begin(21, 22);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Iniciando...");

    Connect_to_WiFi();

    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) Connect_to_WiFi();
    if (!client.connected()) reconnect_MQTT();
    client.loop();

    unsigned long now = millis();
    // A cada 5 segundos gera uma nova temperatura aleatória entre 20°C e 35°C
    if (now - lastMsg > 5000) {
        lastMsg = now;

        // Gera o valor aleatório
        int tempAleatoria = 20 + random(0, 16); // Valores de 20 a 35

        // Publica no tópico de temperatura
        String payload = String(tempAleatoria);
        client.publish(topic_temp, payload.c_str());
        
        Serial.print("Temperatura gerada e publicada: ");
        Serial.print(payload);
        Serial.println(" °C");
    }
}
