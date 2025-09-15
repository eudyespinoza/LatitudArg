try {
    document.addEventListener('DOMContentLoaded', () => {
        console.log('theme-toggle.js cargado correctamente');
        const themeToggle = document.getElementById('theme-toggle');
        const htmlElement = document.documentElement;
        const navbar = document.querySelector('.navbar');

        if (!themeToggle || !navbar) {
            console.error('Error: themeToggle o navbar no encontrados.', { themeToggle, navbar });
            return;
        }

        // Load saved theme
        if (localStorage.getItem('theme') === 'dark') {
            console.log('Cargando tema oscuro desde localStorage');
            htmlElement.setAttribute('data-theme', 'dark');
            navbar.setAttribute('data-bs-theme', 'dark');
            navbar.classList.remove('bg-light');
            navbar.classList.add('bg-dark');
            themeToggle.innerHTML = '<i class="bi bi-moon-stars-fill"></i>';
        } else {
            console.log('Cargando tema claro por defecto');
        }

        // Toggle theme
        themeToggle.addEventListener('click', () => {
            console.log('Cambio de tema solicitado');
            if (htmlElement.getAttribute('data-theme') === 'light') {
                console.log('Cambiando a tema oscuro');
                htmlElement.setAttribute('data-theme', 'dark');
                navbar.setAttribute('data-bs-theme', 'dark');
                navbar.classList.remove('bg-light');
                navbar.classList.add('bg-dark');
                themeToggle.innerHTML = '<i class="bi bi-moon-stars-fill"></i>';
                localStorage.setItem('theme', 'dark');
            } else {
                console.log('Cambiando a tema claro');
                htmlElement.setAttribute('data-theme', 'light');
                navbar.setAttribute('data-bs-theme', 'light');
                navbar.classList.remove('bg-dark');
                navbar.classList.add('bg-light');
                themeToggle.innerHTML = '<i class="bi bi-sun-fill"></i>';
                localStorage.setItem('theme', 'light');
            }
        });
    });
} catch (error) {
    console.error('Error en theme-toggle.js:', error);
}