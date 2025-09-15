document.addEventListener('DOMContentLoaded', () => {
    const alerts = document.querySelectorAll('.modern-alert');
    alerts.forEach(alert => {
        setTimeout(() => {
            alert.classList.remove('animate__fadeInDown');
            alert.classList.add('animate__fadeOutUp');
            setTimeout(() => {
                alert.remove();
            }, 500); // Tiempo de la animación de salida
        }, 3000); // Desaparece tras 5 segundos
    });
});