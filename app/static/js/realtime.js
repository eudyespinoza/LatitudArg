// WebSocket helper for live updates
// - Exponential backoff (2s → 4s → 8s … up to 30s)
// - Max 10 retries, then gives up
// - Pauses reconnection while the tab is hidden (saves mobile CPU/battery)
// - Returns a controller { close() } to allow manual teardown

export function connectVehicle(vehicleId, onMessage) {
  const proto = window.location.protocol === 'https:' ? 'wss' : 'ws';
  const url = `${proto}://${window.location.host}/ws/vehicle/${vehicleId}/`;

  const MAX_RETRIES = 10;
  const BASE_DELAY  = 2000;   // ms
  const MAX_DELAY   = 30000;  // ms

  let ws            = null;
  let retries       = 0;
  let retryTimer    = null;
  let destroyed     = false;

  function connect() {
    if (destroyed) return;
    ws = new WebSocket(url);

    ws.onmessage = (evt) => {
      try { onMessage(JSON.parse(evt.data)); }
      catch (_) { /* ignore malformed frames */ }
    };

    ws.onopen = () => { retries = 0; };

    ws.onclose = () => {
      if (destroyed) return;
      if (retries >= MAX_RETRIES) return; // give up
      retries++;
      const delay = Math.min(BASE_DELAY * Math.pow(2, retries - 1), MAX_DELAY);
      // Defer reconnect while tab is in background
      if (document.visibilityState === 'hidden') {
        const resume = () => {
          document.removeEventListener('visibilitychange', resume);
          scheduleReconnect(delay);
        };
        document.addEventListener('visibilitychange', resume);
      } else {
        scheduleReconnect(delay);
      }
    };

    ws.onerror = () => { /* onclose will handle retry */ };
  }

  function scheduleReconnect(delay) {
    clearTimeout(retryTimer);
    retryTimer = setTimeout(connect, delay);
  }

  connect();

  // Return a controller so callers can cleanly tear down the socket
  return {
    close() {
      destroyed = true;
      clearTimeout(retryTimer);
      if (ws) ws.close();
    }
  };
}

