// static/js/map.js - Leaflet + OSM
let map, marker;

function initMap(params = {}) {
  const lat = params.lat !== undefined && !isNaN(params.lat) ? parseFloat(params.lat) : -34.6037;
  const lng = params.lng !== undefined && !isNaN(params.lng) ? parseFloat(params.lng) : -58.3816;
  const type = (params.type || 'auto').toLowerCase();
  const speed = params.speed || 0.0;
  const lastUpdated = params.lastUpdated || '';
  const signalQuality = params.signal_quality || 0;

  const mapContainer = document.getElementById('map');
  if (!mapContainer) { console.error('Map container not found'); return; }

  try {
    map = L.map(mapContainer).setView([lat, lng], 15);
    window.map = map;
    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
      maxZoom: 19,
      attribution: '&copy; OpenStreetMap contributors'
    }).addTo(map);
  } catch (e) {
    console.error('Error creating Leaflet map:', e);
    mapContainer.innerHTML = '<p class="text-danger">Error al cargar el mapa.</p>';
    return;
  }

  let iconUrl = '/static/img/auto.png';
  if (type === 'moto') iconUrl = '/static/img/moto.png';
  if (type === 'barco') iconUrl = '/static/img/barco.png';
  if (type === 'perro') iconUrl = '/static/img/perro.png';
  if (type === 'gato') iconUrl = '/static/img/gato.png';

  const icon = L.icon({ iconUrl, iconSize: [40, 40], iconAnchor: [20, 20] });
  try {
    marker = L.marker([lat, lng], { icon }).addTo(map);
    window.marker = marker;
  } catch (e) { console.error('Error creating marker:', e); return; }

  const speedElement = document.getElementById('speed');
  const lastUpdatedElement = document.getElementById('last-updated');
  const signalElement = document.getElementById('signal-quality');
  if (speedElement) speedElement.textContent = `${Number(speed).toFixed(2)} km/h`;
  if (lastUpdatedElement) lastUpdatedElement.textContent = lastUpdated;
  if (signalElement) {
    const signal = parseInt(signalQuality);
    signalElement.className = `signal-bars ${signal < 13 ? 'low' : signal < 25 ? 'medium' : 'high'}`;
    signalElement.innerHTML = (
      signal <= 6 ? '<i class="bi bi-reception-0"></i>' :
      signal <= 12 ? '<i class=\"bi bi-reception-1\"></i>' :
      signal <= 18 ? '<i class=\"bi bi-reception-2\"></i>' :
      signal <= 24 ? '<i class=\"bi bi-reception-3\"></i>' :
                     '<i class=\"bi bi-reception-4\"></i>'
    );
  }
}

function updateMarkerPosition(lat, lng) {
  if (window.marker && window.map) {
    const p = [parseFloat(lat), parseFloat(lng)];
    window.marker.setLatLng(p);
    window.map.setView(p);
  } else {
    console.warn('Marker/map not defined');
  }
}

window.updateMarkerPosition = updateMarkerPosition;
