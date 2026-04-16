import paho.mqtt.client as mqtt
import sqlite3
import json
from datetime import datetime

db_name = "can_reader.db"
def start_db():
    con = sqlite3.connect(db_name)
    cur = con.cursor()
    cur.execute('''
            CREATE TABLE IF NOT EXISTS telemetria (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                horario_recebimento DATETIME,
                ts_esp INTEGER,
                voltagem REAL,
                corrente REAL,
                soc INTEGER,
                rpm INTEGER,
                torque REAL,
                modo TEXT,
                temp_bat INTEGER,
                temp_motor INTEGER,
                temp_controller INTEGER
            )
        ''')
    con.commit()
    con.close()


localhost = "192.168.1.33"
port = 1883
topic = "moto/telemetria"

# 1. Callback quando conecta ao broker
def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print(f"Conectado ao Broker! (Código: {reason_code})")
        client.subscribe(topic)
        print(f"Subscribe com sucesso no tópico: {topic}")
    else:
        print(f"Falha na conexão. Código {reason_code}")

# 2. Callback quando uma mensagem chega
def on_message(client, userdata, message):
    try:
        payload = message.payload.decode('utf-8')
        dados = json.loads(payload)
        agora = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        # Salva no SQLite
        con = sqlite3.connect(db_name)
        con.cursor()
        query = '''
                    INSERT INTO telemetria (
                        horario_recebimento, ts_esp, voltagem, corrente, soc, 
                        rpm, torque, modo, temp_bat, temp_motor, temp_controller
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                '''
        
        valores = (
            agora,
            dados.get('ts'),
            dados.get('v'),
            dados.get('a'),
            dados.get('soc'),
            dados.get('rpm'),
            dados.get('tq'),
            dados.get('mod'),
            dados.get('tB'),
            dados.get('tM'),
            dados.get('tC')
        )

        con.execute(query, valores)
        con.commit()
        con.close()

        print(f"[{agora}] Telemetria salva: SOC: {dados.get('soc')}% | RPM: {dados.get('rpm')} | Modo: {dados.get('mod')}")

    except Exception as e:
        print(f"Erro ao processar mensagem: {e}")


# Configuração do Cliente
start_db()

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, "MoDCS_Listener")
client.on_connect = on_connect
client.on_message = on_message
print(f"Tentando conectar ao broker em {localhost}...")

try:
    client.connect(localhost, port, keepalive= 60)
    client.loop_forever()

except KeyboardInterrupt:
    print("\nEncerrando o listener...")
    client.disconnect()

except Exception as e:
    print(f"Erro de conexão: {e}")