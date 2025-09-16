import json
import logging
from datetime import datetime
from django.conf import settings
from django.contrib import messages
from django.contrib.auth import authenticate, login, logout
from django.contrib.auth.decorators import login_required
from django.contrib.auth.hashers import make_password
from django.core.mail import send_mail
from django.utils import timezone
from django.http import JsonResponse, HttpResponseRedirect
from django.views.decorators.csrf import csrf_exempt
from django.shortcuts import redirect, render
from django.urls import reverse
from asgiref.sync import async_to_sync
from channels.layers import get_channel_layer

from .models import User, Vehicle, LocationHistory, ContactRequest
from .mongo import get_db as get_mongo


logger = logging.getLogger(__name__)

def index(request):
    if request.user.is_authenticated and not request.GET.get('landing'):
        return redirect('dashboard')
    context = {
        'contact_email': getattr(settings, 'CONTACT_EMAIL', 'contacto@latitudarg.com'),
    }
    return render(request, 'landing.html', context)

def login_view(request):
    if request.user.is_authenticated:
        return redirect('dashboard')
    if request.method == 'POST':
        username = request.POST.get('username')
        password = request.POST.get('password')
        user = authenticate(request, username=username, password=password)
        if user is not None:
            login(request, user)
            return redirect('dashboard')
        messages.error(request, 'Credenciales inválidas.')
    return render(request, 'login.html')


def contact_view(request):
    target = f"{reverse('index')}?landing=1#contact"
    if request.method != 'POST':
        return redirect(target)

    name = (request.POST.get('name') or '').strip()
    email = (request.POST.get('email') or '').strip()
    phone = (request.POST.get('phone') or '').strip()
    company = (request.POST.get('company') or '').strip()
    message_text = (request.POST.get('message') or '').strip()

    if not name or not email or not message_text:
        messages.error(request, 'Por favor completa nombre, correo y mensaje para que podamos contactarte.')
        return redirect(target)

    contact = ContactRequest.objects.create(
        name=name,
        email=email,
        phone=phone,
        company=company,
        message=message_text,
    )

    contact_email = getattr(settings, 'CONTACT_EMAIL', 'contacto@latitudarg.com')
    if contact_email:
        subject = f"Nueva consulta de {name}"
        from_email = getattr(settings, 'DEFAULT_FROM_EMAIL', contact_email)
        body_lines = [
            f"Nombre: {name}",
            f"Email: {email}",
            f"Telefono: {phone or 'No indicado'}",
            f"Empresa: {company or 'No indicada'}",
            '',
            'Mensaje:',
            message_text,
            '',
            f"ID interno: {contact.id}",
        ]
        body = "\n".join(body_lines)
        try:
            send_mail(subject, body, from_email, [contact_email])
        except Exception as exc:
            logger.warning('No se pudo enviar la notificacion de contacto: %s', exc)

    messages.success(request, 'Gracias por tu consulta. Nuestro equipo te contactara a la brevedad.')
    return redirect(target)

@login_required
def dashboard(request):
    vehicles = Vehicle.objects.filter(user_id=request.user.id)
    return render(request, 'dashboard.html', {'vehicles': vehicles})


@login_required
def admin_panel(request):
    if request.user.role != 'admin':
        messages.error(request, 'Acceso denegado.')
        return redirect('dashboard')
    if request.method == 'POST':
        if 'add_user' in request.POST:
            username = request.POST.get('username')
            email = request.POST.get('email')
            password = request.POST.get('password')
            keyword = request.POST.get('keyword', '')
            if User.objects.filter(username=username).exists():
                messages.error(request, 'El usuario ya existe.')
            else:
                User.objects.create_user(username=username, email=email, password=password, role='user', keyword=keyword)
                messages.success(request, 'Usuario agregado.')
        elif 'add_vehicle' in request.POST:
            user_id = request.POST.get('user_id')
            vehicle_name = request.POST.get('vehicle_name')
            vehicle_type = request.POST.get('vehicle_type')
            patente = request.POST.get('patente')
            device_id = request.POST.get('device_id')
            device_phone = (request.POST.get('device_phone') or '').strip()
            if not all([user_id, vehicle_name, vehicle_type, patente, device_id]):
                messages.error(request, 'Todos los campos son obligatorios.')
                return redirect('admin_panel')
            if Vehicle.objects.filter(device_id=device_id).exists():
                messages.error(request, 'El ID del dispositivo ya está en uso.')
                return redirect('admin_panel')
            Vehicle.objects.create(
                user_id=user_id, name=vehicle_name, type=vehicle_type,
                patente=patente, device_id=device_id, device_phone=device_phone, status='active',
                lat=-34.6037, lng=-58.3816
            )
            messages.success(request, 'Vehículo agregado.')
        elif 'toggle_contact' in request.POST:
            contact_id = request.POST.get('contact_id')
            try:
                cr = ContactRequest.objects.get(id=contact_id)
                cr.handled = not cr.handled
                cr.handled_at = timezone.now() if cr.handled else None
                cr.save(update_fields=['handled', 'handled_at'])
                estado = 'marcada como atendida' if cr.handled else 'marcada como pendiente'
                messages.success(request, f'Consulta {estado}.')
            except ContactRequest.DoesNotExist:
                messages.error(request, 'La consulta indicada no existe.')
        elif 'delete_contact' in request.POST:
            contact_id = request.POST.get('contact_id')
            ContactRequest.objects.filter(id=contact_id).delete()
            messages.success(request, 'Consulta eliminada.')
    users = User.objects.all()
    contacts = ContactRequest.objects.all()
    vehicles = Vehicle.objects.all()
    return render(request, 'admin_panel.html', {'users': users, 'vehicles': vehicles, 'contacts': contacts})


@login_required
def profile(request):
    if request.method == 'POST':
        keyword = request.POST.get('keyword', '')
        old_password = request.POST.get('old_password', '')
        new_password = request.POST.get('new_password', '')
        confirm_password = request.POST.get('confirm_password', '')
        u: User = request.user  # type: ignore
        u.keyword = keyword
        if old_password or new_password or confirm_password:
            if not old_password:
                messages.error(request, 'Debes ingresar tu contraseña actual para cambiar la contraseña.')
                return redirect('profile')
            if new_password != confirm_password:
                messages.error(request, 'Las nuevas contraseñas no coinciden.')
                return redirect('profile')
            if not u.check_password(old_password):
                messages.error(request, 'La contraseña actual es incorrecta.')
                return redirect('profile')
            u.set_password(new_password)
        u.save()
        messages.success(request, 'Perfil actualizado.')
        return redirect('profile')
    return render(request, 'profile.html')


@login_required
def delete_user(request):
    if request.user.role != 'admin':
        messages.error(request, 'Acceso denegado.')
        return redirect('dashboard')
    user_id = request.POST.get('user_id')
    if not user_id:
        messages.error(request, 'ID de usuario no proporcionado.')
        return redirect('admin_panel')
    if str(request.user.id) == str(user_id):
        messages.error(request, 'No puedes eliminar tu propio usuario.')
        return redirect('admin_panel')
    User.objects.filter(id=user_id).delete()
    messages.success(request, 'Usuario y sus vehículos eliminados.')
    return redirect('admin_panel')


@login_required
def update_user(request):
    if request.user.role != 'admin':
        messages.error(request, 'Acceso denegado.')
        return redirect('dashboard')
    user_id = request.POST.get('user_id')
    username = request.POST.get('username')
    email = request.POST.get('email')
    password = request.POST.get('password')
    role = request.POST.get('role', 'user')
    keyword = request.POST.get('keyword', '')
    try:
        u = User.objects.get(id=user_id)
        u.username = username
        u.email = email
        u.role = role
        u.keyword = keyword
        if password:
            u.set_password(password)
        u.save()
        messages.success(request, 'Usuario actualizado.')
    except User.DoesNotExist:
        messages.error(request, 'Usuario no encontrado.')
    return redirect('admin_panel')


@login_required
def delete_vehicle(request):
    if request.user.role != 'admin':
        messages.error(request, 'Acceso denegado.')
        return redirect('dashboard')
    vehicle_id = request.POST.get('vehicle_id')
    if not vehicle_id:
        messages.error(request, 'ID de vehículo no proporcionado.')
        return redirect('admin_panel')
    Vehicle.objects.filter(id=vehicle_id).delete()
    messages.success(request, 'Vehículo eliminado.')
    return redirect('admin_panel')


@login_required
def update_vehicle(request):
    if request.user.role != 'admin':
        messages.error(request, 'Acceso denegado.')
        return redirect('dashboard')
    vehicle_id = request.POST.get('vehicle_id')
    user_id = request.POST.get('user_id')
    name = request.POST.get('name')
    vehicle_type = request.POST.get('vehicle_type')
    patente = request.POST.get('patente')
    device_id = request.POST.get('device_id')
    device_phone = (request.POST.get('device_phone') or '').strip()
    lat = request.POST.get('lat')
    lng = request.POST.get('lng')
    status = request.POST.get('status')
    try:
        v = Vehicle.objects.get(id=vehicle_id)
        v.user_id = user_id
        v.name = name
        v.type = vehicle_type
        v.patente = patente
        v.device_id = device_id or None
        v.device_phone = device_phone
        v.lat = float(lat)
        v.lng = float(lng)
        v.status = status
        v.speed = 0.0
        v.save()
        messages.success(request, 'Vehículo actualizado.')
    except Exception as e:
        messages.error(request, f'Error al actualizar vehículo: {e}')
    return redirect('admin_panel')


@login_required
def vehicle_map(request):
    vehicle_id = request.GET.get('vehicle_id')
    if not vehicle_id:
        messages.error(request, 'ID de vehículo no proporcionado.')
        return redirect('dashboard')
    try:
        vehicle = Vehicle.objects.get(id=vehicle_id, user_id=request.user.id)
    except Vehicle.DoesNotExist:
        messages.error(request, 'Vehículo no encontrado.')
        return redirect('dashboard')
    vehicle_dict = {
        'id': vehicle.id,
        'lat': vehicle.lat if vehicle.lat else -34.6037,
        'lng': vehicle.lng if vehicle.lng else -58.3816,
        'name': vehicle.name,
        'type': vehicle.type,
        'patente': vehicle.patente,
        'last_updated': vehicle.last_updated or 'N/A',
        'signal_quality': vehicle.signal_quality,
        'vehicle_on': vehicle.vehicle_on,
        'shutdown': vehicle.shutdown,
        'transmit_audio': vehicle.transmit_audio,
        'speed': vehicle.speed,
    }
    return render(request, 'vehicle_map.html', {'vehicle': vehicle_dict})


@login_required
def vehicle_history(request):
    vehicle_id = request.GET.get('vehicle_id')
    if not vehicle_id:
        messages.error(request, 'ID de vehículo no proporcionado.')
        return redirect('dashboard')
    try:
        vehicle = Vehicle.objects.get(id=vehicle_id, user_id=request.user.id)
    except Vehicle.DoesNotExist:
        messages.error(request, 'Vehículo no encontrado.')
        return redirect('dashboard')
    default_from = (datetime.now().replace(hour=0, minute=0, second=0, microsecond=0))
    default_to = datetime.now()
    ctx = {
        'vehicle': vehicle,
        'default_from': default_from.strftime('%Y-%m-%dT%H:%M'),
        'default_to': default_to.strftime('%Y-%m-%dT%H:%M'),
    }
    return render(request, 'vehicle_history.html', ctx)


def broadcast_vehicle(vehicle_id: int, payload: dict):
    channel_layer = get_channel_layer()
    async_to_sync(channel_layer.group_send)(
        f"vehicle_{vehicle_id}",
        {"type": "vehicle.event", "data": payload}
    )


@csrf_exempt
def api_update_location(request):
    if request.method != 'POST':
        return JsonResponse({'status': 'error', 'message': 'Método no permitido'}, status=405)
    try:
        data = json.loads(request.body.decode('utf-8'))
    except Exception:
        return JsonResponse({'status': 'error', 'message': 'JSON inválido'}, status=400)

    device_id = data.get('device_id')
    lat = data.get('lat')
    lng = data.get('lng')
    speed = data.get('speed', 0.0)
    signal_quality = data.get('signal_quality', 0)
    vehicle_on = data.get('vehicle_on', False)

    if not all([device_id, lat, lng]):
        return JsonResponse({'status': 'error', 'message': 'Faltan datos requeridos.'}, status=400)
    try:
        lat = float(lat)
        lng = float(lng)
        speed = float(speed)
        signal_quality = int(signal_quality)
    except Exception:
        return JsonResponse({'status': 'error', 'message': 'Lat/Lng/Velocidad/Señal inválidos.'}, status=400)

    try:
        v = Vehicle.objects.get(device_id=device_id)
    except Vehicle.DoesNotExist:
        return JsonResponse({'status': 'error', 'message': 'Dispositivo no encontrado.'}, status=404)

    # actualizar
    now_str = datetime.now().strftime('%d-%m-%Y %H:%M')
    if lat != 0.0 or lng != 0.0:
        v.lat = lat
        v.lng = lng
        v.speed = speed
        v.signal_quality = signal_quality
        v.vehicle_on = bool(vehicle_on)
        v.last_updated = now_str
        v.save()
        LocationHistory.objects.create(
            vehicle=v, lat=lat, lng=lng, speed=speed,
            signal_quality=signal_quality, vehicle_on=bool(vehicle_on)
        )
        try:
            mdb = get_mongo()
            mdb.location_history.insert_one({
                'vehicle_id': v.id,
                'lat': lat,
                'lng': lng,
                'speed': speed,
                'signal_quality': signal_quality,
                'vehicle_on': bool(vehicle_on),
                'timestamp': datetime.utcnow(),
            })
            mdb.vehicles.update_one(
                {'vehicle_id': v.id},
                {'$set': {
                    'lat': lat,
                    'lng': lng,
                    'speed': speed,
                    'signal_quality': signal_quality,
                    'vehicle_on': bool(vehicle_on),
                    'last_updated': now_str,
                }},
                upsert=True
            )
        except Exception:
            pass

    # broadcast
    payload = {
        'vehicle_id': str(v.id),
        'lat': lat,
        'lng': lng,
        'speed': speed,
        'signal_quality': signal_quality,
        'vehicle_on': bool(vehicle_on),
        'shutdown': bool(v.shutdown),
        'transmit_audio': bool(v.transmit_audio),
        'last_updated': now_str,
    }
    try:
        broadcast_vehicle(v.id, payload)
    except Exception:
        pass

    return JsonResponse({
        'status': 'success',
        'shutdown': bool(v.shutdown),
        'transmit_audio': bool(v.transmit_audio),
        'last_updated': now_str,
    })


@login_required
def api_vehicle_history(request, vehicle_id: int):
    # formateo fechas: from=YYYY-MM-DD[THH:MM], to=...
    src = request.GET.get('source', 'mongo')
    fmt = request.GET.get('format', 'json')
    from_s = request.GET.get('from')
    to_s = request.GET.get('to')
    try:
        Vehicle.objects.get(id=vehicle_id, user_id=request.user.id)
    except Vehicle.DoesNotExist:
        return JsonResponse({'status': 'error', 'message': 'Vehículo no encontrado.'}, status=404)

    def parse_dt(s, default):
        if not s:
            return default
        for f in ('%Y-%m-%dT%H:%M', '%Y-%m-%d'):
            try:
                return datetime.strptime(s, f)
            except Exception:
                continue
        return default

    default_from = datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
    default_to = datetime.now()
    dt_from = parse_dt(from_s, default_from)
    dt_to = parse_dt(to_s, default_to)

    # Ajuste a UTC para Mongo (guardamos con datetime.utcnow())
    try:
        tz_offset = datetime.utcnow() - datetime.now()
    except Exception:
        tz_offset = None
    dt_from_utc = dt_from + tz_offset if tz_offset else dt_from
    dt_to_utc = dt_to + tz_offset if tz_offset else dt_to

    points = []
    if src == 'mongo':
        try:
            mdb = get_mongo()
            cur = mdb.location_history.find({
                'vehicle_id': vehicle_id,
                'timestamp': {'$gte': dt_from_utc, '$lte': dt_to_utc}
            }).sort('timestamp', 1)
            for doc in cur:
                points.append({
                    'lat': float(doc.get('lat', 0)),
                    'lng': float(doc.get('lng', 0)),
                    'speed': float(doc.get('speed', 0)),
                    'signal_quality': int(doc.get('signal_quality', 0)),
                    'vehicle_on': bool(doc.get('vehicle_on', False)),
                    'timestamp': doc.get('timestamp').isoformat() if doc.get('timestamp') else ''
                })
        except Exception:
            # fall back to sqlite
            src = 'sqlite'

    # Fallback: si Mongo no trajo puntos, probar en SQLite también
    if not points and src == 'mongo':
        qs = LocationHistory.objects.filter(vehicle_id=vehicle_id, timestamp__gte=dt_from, timestamp__lte=dt_to).order_by('timestamp')
        for r in qs:
            points.append({
                'lat': float(r.lat), 'lng': float(r.lng), 'speed': float(r.speed),
                'signal_quality': int(r.signal_quality), 'vehicle_on': bool(r.vehicle_on),
                'timestamp': r.timestamp.isoformat()
            })

    if src == 'sqlite':
        qs = LocationHistory.objects.filter(vehicle_id=vehicle_id, timestamp__gte=dt_from, timestamp__lte=dt_to).order_by('timestamp')
        for r in qs:
            points.append({
                'lat': float(r.lat), 'lng': float(r.lng), 'speed': float(r.speed),
                'signal_quality': int(r.signal_quality), 'vehicle_on': bool(r.vehicle_on),
                'timestamp': r.timestamp.isoformat()
            })

    if fmt == 'csv':
        import csv
        from io import StringIO
        sio = StringIO()
        w = csv.writer(sio)
        w.writerow(['timestamp','lat','lng','speed','signal_quality','vehicle_on'])
        for p in points:
            w.writerow([p['timestamp'], p['lat'], p['lng'], p['speed'], p['signal_quality'], int(p['vehicle_on'])])
        from django.http import HttpResponse
        resp = HttpResponse(sio.getvalue(), content_type='text/csv')
        resp['Content-Disposition'] = f'attachment; filename="vehicle_{vehicle_id}_history.csv"'
        return resp

    return JsonResponse({'status': 'success', 'points': points})


@csrf_exempt
@login_required
def api_shutdown_vehicle(request, vehicle_id: int):
    try:
        v = Vehicle.objects.get(id=vehicle_id, user_id=request.user.id)
    except Vehicle.DoesNotExist:
        return JsonResponse({'status': 'error', 'message': 'Vehículo no encontrado.'}, status=404)
    v.shutdown = not v.shutdown
    v.save()
    payload = {'vehicle_id': str(v.id), 'command': 'shutdown' if v.shutdown else 'turn_on', 'shutdown': bool(v.shutdown)}
    try:
        broadcast_vehicle(v.id, payload)
    except Exception:
        pass
    return JsonResponse({'status': 'success', 'message': 'Ok', 'shutdown': bool(v.shutdown), 'transmit_audio': bool(v.transmit_audio)})


@csrf_exempt
@login_required
def api_toggle_audio(request, vehicle_id: int):
    try:
        v = Vehicle.objects.get(id=vehicle_id, user_id=request.user.id)
    except Vehicle.DoesNotExist:
        return JsonResponse({'status': 'error', 'message': 'Vehículo no encontrado.'}, status=404)
    v.transmit_audio = not v.transmit_audio
    v.save()
    payload = {'vehicle_id': str(v.id), 'command': 'transmit_audio' if v.transmit_audio else 'stop_audio', 'transmit_audio': bool(v.transmit_audio)}
    try:
        broadcast_vehicle(v.id, payload)
    except Exception:
        pass
    return JsonResponse({'status': 'success', 'message': 'Ok', 'transmit_audio': bool(v.transmit_audio), 'audio_url': 'simulated_audio.mp3' if v.transmit_audio else None})


def logout_view(request):
    logout(request)
    return redirect('login')
