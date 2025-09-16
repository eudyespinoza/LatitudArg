document.addEventListener('DOMContentLoaded', () => {
    // Initialize Socket.IO
    const socket = io({
        transports: ['websocket', 'polling'],
        reconnection: true,
        reconnectionAttempts: 5,
        reconnectionDelay: 1000
    });

    // Store the current vehicle ID
    let currentVehicleId = null;

    // Function to join a vehicle room
    window.joinVehicleRoom = function(vehicleId) {
        if (!vehicleId) {
            console.error('Vehicle ID is not provided');
            showNotification('Error: ID de vehículo no proporcionado', 'danger');
            return;
        }
        currentVehicleId = vehicleId.toString();
        socket.emit('join', vehicleId);
        console.log(`Joined room for vehicle ID: ${vehicleId}`);
    };

    // Handle connection errors
    socket.on('connect_error', (error) => {
        console.error('Socket.IO connection error:', error);
        showNotification(`Error de conexión WebSocket: ${error}`, 'danger');
    });

    // Handle successful connection
    socket.on('connect', () => {
        console.log('Connected to Socket.IO server');
        if (currentVehicleId) {
            window.joinVehicleRoom(currentVehicleId);
        }
    });

    // Handle location update event
    socket.on('location_update', (data) => {
        console.log('Received location_update:', data);
        const vehicleId = data.vehicle_id.toString();

        // Use last_updated directly from backend (DD-MM-YYYY HH:MM)
        const lastUpdated = data.last_updated || new Date().toLocaleString('es-AR', {
            day: '2-digit',
            month: '2-digit',
            year: 'numeric',
            hour: '2-digit',
            minute: '2-digit',
            hour12: false
        }).replace(',', '').replace(/\//g, '-');
        console.log(`Processed last_updated: ${lastUpdated}`);

        // Update vehicle_map.html
        console.log(`Checking if vehicleId=${vehicleId} matches currentVehicleId=${currentVehicleId}`);
        if (vehicleId === currentVehicleId) {
            const speedElement = document.getElementById('speed');
            const lastUpdatedElement = document.getElementById('last-updated');
            const signalElement = document.getElementById('signal-quality');
            const vehicleState = document.getElementById('vehicle-state');
            const controlState = document.getElementById('control-state');
            const audioState = document.getElementById('audio-state');
            const audioToggle = document.getElementById('audioToggle');
            if (window.marker && window.map) {
                const newPos = {
                    lat: parseFloat(data.lat),
                    lng: parseFloat(data.lng)
                };
                if (!isNaN(newPos.lat) && !isNaN(newPos.lng)) {
                    window.marker.setPosition(newPos);
                    window.map.setCenter(newPos);
                    console.log('Mapa actualizado:', newPos);
                } else {
                    console.warn('Coordenadas inválidas:', newPos);
                }
            }
            if (speedElement) {
                speedElement.textContent = `${(data.speed || 0.0).toFixed(2)} km/h`;
                console.log(`Updated speed: ${(data.speed || 0.0).toFixed(2)} km/h`);
            } else {
                console.warn('speed element not found');
            }
            if (lastUpdatedElement) {
                lastUpdatedElement.textContent = lastUpdated;
                console.log(`Updated last-updated on vehicle_map.html to: ${lastUpdated}`);
            } else {
                console.warn('last-updated element not found in vehicle_map.html');
            }
            if (signalElement) {
                const signal = parseInt(data.signal_quality || 0);
                signalElement.className = `signal-bars ${signal < 13 ? 'low' : signal < 25 ? 'medium' : 'high'}`;
                signalElement.innerHTML = `
                    ${signal <= 6 ? '<i class="bi bi-reception-0"></i>' :
                      signal <= 12 ? '<i class="bi bi-reception-1"></i>' :
                      signal <= 18 ? '<i class="bi bi-reception-2"></i>' :
                      signal <= 24 ? '<i class="bi bi-reception-3"></i>' :
                      '<i class="bi bi-reception-4"></i>'}
                `;
                console.log(`Updated signal-quality: ${signal}`);
            } else {
                console.warn('signal-quality element not found');
            }
            if (vehicleState) {
                vehicleState.className = `badge bg-${data.vehicle_on ? 'success' : 'danger'}`;
                vehicleState.textContent = data.vehicle_on ? 'Encendido' : 'Apagado';
                console.log(`Updated vehicle-state: ${data.vehicle_on ? 'Encendido' : 'Apagado'}`);
            } else {
                console.warn('vehicle-state element not found');
            }
            if (controlState) {
                controlState.className = `badge bg-${data.shutdown ? 'danger' : 'success'}`;
                controlState.textContent = data.shutdown ? 'Apagado' : 'Encendido';
                console.log(`Updated control-state: ${data.shutdown ? 'Apagado' : 'Encendido'}`);
            } else {
                console.warn('control-state element not found');
            }
            if (audioState && audioToggle) {
                const isTransmitting = data.transmit_audio;
                audioState.className = `badge bg-${isTransmitting ? 'success' : 'danger'}`;
                audioState.textContent = isTransmitting ? 'Activada' : 'Desactivada';
                audioToggle.classList.remove(`btn-outline-${isTransmitting ? 'success' : 'danger'}`);
                audioToggle.classList.add(`btn-outline-${isTransmitting ? 'danger' : 'success'}`);
                audioToggle.querySelector('i').className = `bi ${isTransmitting ? 'bi-volume-mute text-danger' : 'bi-volume-up text-success'} me-1`;
                audioToggle.title = isTransmitting ? 'Desactivar Audio' : 'Activar Audio';
                audioToggle.innerHTML = `<i class="bi ${isTransmitting ? 'bi-volume-mute text-danger' : 'bi-volume-up text-success'} me-1"></i> ${isTransmitting ? 'Desactivar' : 'Activar'} Audio`;
                console.log(`Updated audio-state: ${isTransmitting ? 'Activada' : 'Desactivada'}`);
            } else {
                console.warn('audio-state or audioToggle element not found');
            }
        } else {
            console.warn(`Vehicle ID mismatch: vehicleId=${vehicleId}, currentVehicleId=${currentVehicleId}`);
        }

        // Update dashboard.html
        const vehicleCard = document.querySelector(`.vehicle-card[data-vehicle-id="${vehicleId}"]`);
        if (vehicleCard) {
            const latElement = document.getElementById(`lat-${vehicleId}`);
            const lngElement = document.getElementById(`lng-${vehicleId}`);
            const speedElement = document.getElementById(`speed-${vehicleId}`);
            const signalElement = document.getElementById(`signal-quality-${vehicleId}`);
            const vehicleState = document.getElementById(`vehicle-state-${vehicleId}`);
            const controlState = document.getElementById(`control-state-${vehicleId}`);
            const audioState = document.getElementById(`audio-state-${vehicleId}`);
            const audioToggle = document.getElementById(`audioToggle-${vehicleId}`);
            const lastUpdatedElement = document.getElementById(`last-updated-${vehicleId}`);
            if (latElement && lngElement && speedElement && signalElement && vehicleState && controlState && audioState && audioToggle && lastUpdatedElement) {
                const newPos = {
                    lat: parseFloat(data.lat),
                    lng: parseFloat(data.lng)
                };
                if (!isNaN(newPos.lat) && !isNaN(newPos.lng)) {
                    latElement.textContent = newPos.lat.toFixed(6);
                    lngElement.textContent = newPos.lng.toFixed(6);
                    console.log(`Updated lat-${vehicleId}: ${newPos.lat}, lng-${vehicleId}: ${newPos.lng}`);
                } else {
                    console.warn('Invalid coordinates for dashboard:', newPos);
                }
                speedElement.textContent = `${(data.speed || 0.0).toFixed(2)} km/h`;
                console.log(`Updated speed-${vehicleId}: ${(data.speed || 0.0).toFixed(2)} km/h`);
                const signal = parseInt(data.signal_quality || 0);
                signalElement.className = `signal signal-bars ${signal < 13 ? 'low' : signal < 25 ? 'medium' : 'high'}`;
                signalElement.innerHTML = `
                    ${signal <= 6 ? '<i class="bi bi-reception-0"></i>' :
                      signal <= 12 ? '<i class="bi bi-reception-1"></i>' :
                      signal <= 18 ? '<i class="bi bi-reception-2"></i>' :
                      signal <= 24 ? '<i class="bi bi-reception-3"></i>' :
                      '<i class="bi bi-reception-4"></i>'}
                `;
                console.log(`Updated signal-quality-${vehicleId}: ${signal}`);
                vehicleState.className = `badge bg-${data.vehicle_on ? 'success' : 'danger'}`;
                vehicleState.textContent = data.vehicle_on ? 'Encendido' : 'Apagado';
                console.log(`Updated vehicle-state-${vehicleId}: ${data.vehicle_on ? 'Encendido' : 'Apagado'}`);
                controlState.className = `badge bg-${data.shutdown ? 'danger' : 'success'}`;
                controlState.textContent = data.shutdown ? 'Apagado' : 'Encendido';
                console.log(`Updated control-state-${vehicleId}: ${data.shutdown ? 'Apagado' : 'Encendido'}`);
                audioState.className = `badge bg-${data.transmit_audio ? 'success' : 'danger'}`;
                audioState.textContent = data.transmit_audio ? 'Activada' : 'Desactivada';
                audioToggle.classList.remove(`btn-outline-${data.transmit_audio ? 'success' : 'danger'}`);
                audioToggle.classList.add(`btn-outline-${data.transmit_audio ? 'danger' : 'success'}`);
                audioToggle.querySelector('i').className = `bi ${data.transmit_audio ? 'bi-volume-mute text-danger' : 'bi-volume-up text-success'} me-1`;
                audioToggle.title = data.transmit_audio ? 'Desactivar Audio' : 'Activar Audio';
                audioToggle.innerHTML = `<i class="bi ${data.transmit_audio ? 'bi-volume-mute text-danger' : 'bi-volume-up text-success'} me-1"></i> ${data.transmit_audio ? 'Desactivar' : 'Activar'} Audio`;
                console.log(`Updated audio-state-${vehicleId}: ${data.transmit_audio ? 'Activada' : 'Desactivada'}`);
                lastUpdatedElement.textContent = lastUpdated;
                console.log(`Updated last-updated-${vehicleId} on dashboard.html to: ${lastUpdated}`);
            } else {
                console.warn('One or more elements not found for vehicle ID:', vehicleId);
            }
        }
    });

    // Handle shutdown command event
    socket.on('shutdown_command', (data) => {
        console.log('Received shutdown_command:', data);
        const vehicleId = data.vehicle_id.toString();

        // Update vehicle_map.html
        if (vehicleId === currentVehicleId) {
            const toggle = document.getElementById('shutdownToggle');
            const controlState = document.getElementById('control-state');
            if (toggle && controlState) {
                toggle.classList.remove(`btn-outline-${data.shutdown ? 'success' : 'danger'}`);
                toggle.classList.add(`btn-outline-${data.shutdown ? 'danger' : 'success'}`);
                toggle.querySelector('i').className = `bi bi-power ${data.shutdown ? 'text-danger' : 'text-success'} me-1`;
                toggle.title = data.shutdown ? 'Encender Vehículo' : 'Apagar Vehículo';
                toggle.innerHTML = `<i class="bi bi-power ${data.shutdown ? 'text-danger' : 'text-success'} me-1"></i> ${data.shutdown ? 'Encender' : 'Apagar'}`;
                controlState.className = `badge bg-${data.shutdown ? 'danger' : 'success'}`;
                controlState.textContent = data.shutdown ? 'Apagado' : 'Encendido';
                showNotification(`El vehículo ha sido ${data.shutdown ? 'apagado' : 'encendido'}.`, 'primary');
            }
        }

        // Update dashboard.html
        const toggle = document.getElementById(`shutdownToggle-${vehicleId}`);
        const controlState = document.getElementById(`control-state-${vehicleId}`);
        if (toggle && controlState) {
            toggle.classList.remove(`btn-outline-${data.shutdown ? 'success' : 'danger'}`);
            toggle.classList.add(`btn-outline-${data.shutdown ? 'danger' : 'success'}`);
            toggle.querySelector('i').className = `bi bi-power ${data.shutdown ? 'text-danger' : 'text-success'} me-1`;
            toggle.title = data.shutdown ? 'Encender Vehículo' : 'Apagar Vehículo';
            toggle.innerHTML = `<i class="bi bi-power ${data.shutdown ? 'text-danger' : 'text-success'} me-1"></i> ${data.shutdown ? 'Encender' : 'Apagar'}`;
            controlState.className = `badge bg-${data.shutdown ? 'danger' : 'success'}`;
            controlState.textContent = data.shutdown ? 'Apagado' : 'Encendido';
            showNotification(`El vehículo ha sido ${data.shutdown ? 'apagado' : 'encendido'}.`, 'primary');
        }
    });

    // Handle audio command event
    socket.on('audio_command', (data) => {
        console.log('Received audio_command:', data);
        const vehicleId = data.vehicle_id.toString();
        const isTransmitting = data.command === 'transmit_audio';

        // Update vehicle_map.html
        if (vehicleId === currentVehicleId) {
            const audioToggle = document.getElementById('audioToggle');
            const audioState = document.getElementById('audio-state');
            if (audioToggle && audioState) {
                audioToggle.classList.remove(`btn-outline-${isTransmitting ? 'success' : 'danger'}`);
                audioToggle.classList.add(`btn-outline-${isTransmitting ? 'danger' : 'success'}`);
                audioToggle.querySelector('i').className = `bi ${isTransmitting ? 'bi-volume-mute text-danger' : 'bi-volume-up text-success'} me-1`;
                audioToggle.title = isTransmitting ? 'Desactivar Audio' : 'Activar Audio';
                audioToggle.innerHTML = `<i class="bi ${isTransmitting ? 'bi-volume-mute text-danger' : 'bi-volume-up text-success'} me-1"></i> ${isTransmitting ? 'Desactivar' : 'Activar'} Audio`;
                audioState.className = `badge bg-${isTransmitting ? 'success' : 'danger'}`;
                audioState.textContent = isTransmitting ? 'Activada' : 'Desactivada';
                showNotification(`Transmisión de audio ${isTransmitting ? 'activada' : 'desactivada'}.`, 'primary');
            }
        }

        // Update dashboard.html
        const audioToggle = document.getElementById(`audioToggle-${vehicleId}`);
        const audioState = document.getElementById(`audio-state-${vehicleId}`);
        if (audioToggle && audioState) {
            audioToggle.classList.remove(`btn-outline-${isTransmitting ? 'success' : 'danger'}`);
            audioToggle.classList.add(`btn-outline-${isTransmitting ? 'danger' : 'success'}`);
            audioToggle.querySelector('i').className = `bi ${isTransmitting ? 'bi-volume-mute text-danger' : 'bi-volume-up text-success'} me-1`;
            audioToggle.title = isTransmitting ? 'Desactivar Audio' : 'Activar Audio';
            audioToggle.innerHTML = `<i class="bi ${isTransmitting ? 'bi-volume-mute text-danger' : 'bi-volume-up text-success'} me-1"></i> ${isTransmitting ? 'Desactivar' : 'Activar'} Audio`;
            audioState.className = `badge bg-${isTransmitting ? 'success' : 'danger'}`;
            audioState.textContent = isTransmitting ? 'Activada' : 'Desactivada';
            showNotification(`Transmisión de audio ${isTransmitting ? 'activada' : 'desactivada'}.`, 'primary');
        }
    });

    // Utility function to show notifications
    function showNotification(message, type) {
        const notificationContainer = document.getElementById('notification-container');
        if (!notificationContainer) {
            console.warn('Notification container not found');
            return;
        }

        const notification = document.createElement('div');
        notification.className = `alert alert-${type} alert-dismissible fade show modern-alert shadow-sm animate__animated animate__fadeInDown`;
        notification.innerHTML = `
            <i class="bi bi-${type === 'danger' ? 'exclamation-circle' : 'info-circle'} me-2"></i>
            ${message}
            <button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
        `;
        notificationContainer.appendChild(notification);
        setTimeout(() => {
            notification.classList.remove('animate__fadeInDown');
            notification.classList.add('animate__fadeOutUp');
            setTimeout(() => notification.remove(), 500);
        }, 5000);
    }
});