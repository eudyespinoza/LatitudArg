function showShutdownModal(vehicleId) {
    currentVehicleId = vehicleId;
    const modal = new bootstrap.Modal(document.getElementById('shutdownModal'));
    modal.show();
}

function shutdownVehicle() {
    if (!currentVehicleId) return;
    fetch(`/api/vehicle/${currentVehicleId}/shutdown`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' }
    })
    .then(response => response.json())
    .then(data => {
        const notification = document.createElement('div');
        notification.className = `alert alert-${data.status === 'success' ? 'primary' : 'danger'} alert-dismissible fade show modern-alert shadow-sm animate__animated animate__fadeInDown`;
        notification.innerHTML = `
            <i class="bi ${data.status === 'success' ? 'bi-info-circle' : 'bi-exclamation-circle'} me-2"></i>
            ${data.message}
            <button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
        `;
        document.getElementById('notification-container').appendChild(notification);
        setTimeout(() => {
            notification.classList.remove('animate__fadeInDown');
            notification.classList.add('animate__fadeOutUp');
            setTimeout(() => notification.remove(), 500);
        }, 5000);
        if (data.status === 'success') {
            const toggle = document.getElementById('shutdownToggle');
            const controlState = document.getElementById('control-state');
            toggle.classList.remove('btn-outline-' + (data.shutdown ? 'success' : 'danger'));
            toggle.classList.add('btn-outline-' + (data.shutdown ? 'danger' : 'success'));
            toggle.querySelector('i').classList.remove(data.shutdown ? 'text-success' : 'text-danger');
            toggle.querySelector('i').classList.add(data.shutdown ? 'text-danger' : 'text-success');
            toggle.title = data.shutdown ? 'Apagar Vehículo' : 'Encender Vehículo';
            controlState.className = 'badge bg-' + (data.shutdown ? 'danger' : 'success');
            controlState.textContent = data.shutdown ? 'Apagado' : 'Encendido';
        }
    })
    .catch(error => {
        const notification = document.createElement('div');
        notification.className = 'alert alert-danger alert-dismissible fade show modern-alert shadow-sm animate__animated animate__fadeInDown';
        notification.innerHTML = `
            <i class="bi bi-exclamation-circle me-2"></i>
            Error: ${error}
            <button type="button" class="btn-close" data-bs-dismiss="alert" aria-label="Close"></button>
        `;
        document.getElementById('notification-container').appendChild(notification);
        setTimeout(() => {
            notification.classList.remove('animate__fadeInDown');
            notification.classList.add('animate__fadeOutUp');
            setTimeout(() => notification.remove(), 500);
        }, 5000);
    });
    const modal = bootstrap.Modal.getInstance(document.getElementById('shutdownModal'));
    modal.hide();
}

document.addEventListener('DOMContentLoaded', () => {
    const confirmButton = document.getElementById('confirmShutdown');
    if (confirmButton) {
        confirmButton.addEventListener('click', shutdownVehicle);
    }
});