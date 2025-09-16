import os
from django.core.asgi import get_asgi_application
from channels.routing import ProtocolTypeRouter, URLRouter
from channels.auth import AuthMiddlewareStack
from django.conf import settings
import gpsapp.routing

os.environ.setdefault('DJANGO_SETTINGS_MODULE', 'gps_site.settings')

django_asgi_app = get_asgi_application()

application = ProtocolTypeRouter({
    "http": django_asgi_app,
    "websocket": AuthMiddlewareStack(
        URLRouter(gpsapp.routing.websocket_urlpatterns)
    ),
})

