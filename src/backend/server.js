const mqtt = require('mqtt');
const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const cors = require('cors');

const app = express();
app.use(cors());
const server = http.createServer(app);
const io = new Server(server, {
    cors: { 
        origin: "*", // Para testes no hotspot, o "*" libera qualquer origem e evita dor de cabeça
        methods: ["GET", "POST"]
    }
});

const MQTT_BROKER = "mqtt://10.74.236.154";
const TOPIC = "moto/telemetria";

const mqttClient = mqtt.connect(MQTT_BROKER);

mqttClient.on('connect', () => {
    console.log("✓ Node.js conectado ao Broker MQTT");
    mqttClient.subscribe(TOPIC);
});

mqttClient.on('message', (topic, message) => {
    const dados = JSON.parse(message.toString());
   
    io.emit('telemetria_update', dados);
    console.log("Dados repassados para o React:", dados.soc, "% SOC");
});

server.listen(4000, () => {
    console.log("Server rodando na porta 4000");
});