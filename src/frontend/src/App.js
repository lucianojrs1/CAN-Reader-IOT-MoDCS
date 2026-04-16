import React, { useEffect, useState } from 'react';
import io from 'socket.io-client';

// Conecta ao seu servidor Node.js na porta 4000
const socket = io('http://10.74.236.154:4000');

function App() {
  const [telemetria, setTelemetria] = useState(null);

  useEffect(() => {
    // Escuta o evento que criamos no Node.js
    socket.on('telemetria_update', (dados) => {
      setTelemetria(dados);
    });

    return () => socket.off('telemetria_update');
  }, []);

  if (!telemetria) {
    return (
      <div style={{ textAlign: 'center', marginTop: '50px', color: '#666' }}>
        <h2>Aguardando dados da Moto...</h2>
        <p>Certifique-se de que o Node.js e o Broker estão rodando.</p>
      </div>
    );
  }

  return (
    <div style={{ padding: '20px', backgroundColor: '#121212', minHeight: '100vh', color: 'white', fontFamily: 'sans-serif' }}>
      <header style={{ borderBottom: '1px solid #333', marginBottom: '20px' }}>
        <h1>MoDCS Dashboard</h1>
      </header>

      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: '15px' }}>
        <InfoCard label="RPM" value={telemetria.rpm} unit="RPM" color="#3498db" />
        <InfoCard label="Bateria (SOC)" value={telemetria.soc} unit="%" color="#2ecc71" />
        <InfoCard label="Tensão" value={telemetria.v} unit="V" color="#f1c40f" />
        <InfoCard label="Corrente" value={telemetria.a} unit="A" color="#e67e22" />
        <InfoCard label="Modo" value={telemetria.mod} unit="" color="#9b59b6" />
      </div>

      <div style={{ marginTop: '30px', display: 'flex', gap: '20px' }}>
        <TempLabel label="Motor" value={telemetria.tM} />
        <TempLabel label="Bateria" value={telemetria.tB} />
        <TempLabel label="Controller" value={telemetria.tC} />
      </div>
    </div>
  );
}

// Componente para os cartões de dados
const InfoCard = ({ label, value, unit, color }) => (
  <div style={{ backgroundColor: '#1e1e1e', padding: '20px', borderRadius: '12px', borderLeft: `5px solid ${color}` }}>
    <p style={{ margin: 0, fontSize: '14px', color: '#aaa' }}>{label}</p>
    <h2 style={{ margin: '10px 0 0 0', fontSize: '32px' }}>{value} <span style={{ fontSize: '16px' }}>{unit}</span></h2>
  </div>
);

// Componente para as temperaturas
const TempLabel = ({ label, value }) => (
  <div style={{ backgroundColor: '#252525', padding: '10px 20px', borderRadius: '8px' }}>
    <span>Temp. {label}: <strong>{value}°C</strong></span>
  </div>
);

export default App;