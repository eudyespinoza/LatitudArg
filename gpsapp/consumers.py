import json
from channels.generic.websocket import AsyncWebsocketConsumer


class VehicleConsumer(AsyncWebsocketConsumer):
    async def connect(self):
        self.vehicle_id = self.scope['url_route']['kwargs']['vehicle_id']
        self.group_name = f"vehicle_{self.vehicle_id}"
        await self.channel_layer.group_add(self.group_name, self.channel_name)
        await self.accept()

    async def disconnect(self, code):
        await self.channel_layer.group_discard(self.group_name, self.channel_name)

    async def receive(self, text_data=None, bytes_data=None):
        # Client-to-server not used currently
        pass

    async def vehicle_event(self, event):
        data = event.get('data', {})
        await self.send(text_data=json.dumps(data))

