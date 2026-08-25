# MQTT-Teste
---
MQTT explorer & Eclipse Mosquitto

*Windows*
---
*Para abrir o servidor do Eclipse Mosquitto
-no terminal/powershell
	cd "C:\Users\Administrador\Downloads\softwares\Mosquitto"  // caminho válido para o meu desktop
	.\mosquitto -c mosquitto.conf -v  // pronto, o servido com o usuário e senha está aberto para usá-lo 

*Para enviar coisas para o MQTT explorer via terminal
-no terminal/powershell
	cd "C:\Users\Administrador\Downloads\softwares\Mosquitto" 
	.\mosquitto_pub -h "host do MQTT explorer" -u "nome do usuário"-P "Senha"-t "Nome da pasta que vai ser criada/quem está dizendo" -m "mensagem"
	.\mosquitto_pub -h localhost -u Pedro_esp32 -P 2101 -t teste/computador -m "Ola do terminal para o MQTT Explorer!" // meu caso teste


*Para encontrar o endereço IPv4* 
* no terminal;
	ipconfig
-192.168.1.107 // meu PC na minha rede Bar do pedro

* Para rodar o código em Python
* No terminal com o caminho onde está o arquivo
	- uv run python "nome do arquivo.py"
		uv run python app_mqtt.py // meu caso para o 'extraindo informações mqtt'
