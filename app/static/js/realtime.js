// Simple WebSocket helper for live updates

export function connectVehicle(vehicleId, onMessage) {
  const proto = window.location.protocol === 'https:' ? 'wss' : 'ws';
  const url = `${proto}://${window.location.host}/ws/vehicle/${vehicleId}/`;
  let ws = new WebSocket(url);

  ws.onopen = () => console.log('WS connected for vehicle', vehicleId);
  ws.onmessage = (evt) => {
    try {
      const data = JSON.parse(evt.data);
      onMessage(data);
    } catch (e) { console.error('WS parse error', e); }
  };
  ws.onclose = () => {
    console.warn('WS closed; retrying in 2s');
    setTimeout(() => connectVehicle(vehicleId, onMessage), 2000);
  };
  ws.onerror = (err) => console.error('WS error', err);
  return ws;
}

