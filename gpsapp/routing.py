from django.urls import re_path
from . import consumers

websocket_urlpatterns = [
    re_path(r'ws/vehicle/(?P<vehicle_id>\d+)/$', consumers.VehicleConsumer.as_asgi()),
]
