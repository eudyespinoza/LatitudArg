from jinja2 import Environment, pass_context
from django.templatetags.static import static as dj_static
from django.urls import reverse
from django.utils.safestring import mark_safe
from django.middleware.csrf import get_token


def environment(**options):
    env = Environment(**options)

    def url_for(endpoint, **kwargs):
        if endpoint == 'static':
            filename = kwargs.get('filename', '')
            return dj_static(filename)

        mapping = {
            'main.index': 'index',
            'main.login': 'login',
            'main.register': 'register',
            'main.contact': 'contact',
            'main.dashboard': 'dashboard',
            'main.admin_panel': 'admin_panel',
            'main.profile': 'profile',
            'main.logout': 'logout',
            'main.delete_user': 'delete_user',
            'main.update_user': 'update_user',
            'main.delete_vehicle': 'delete_vehicle',
            'main.update_vehicle': 'update_vehicle',
            'main.vehicle_map': 'vehicle_map',
            'main.vehicle_history': 'vehicle_history',
        }
        name = mapping.get(endpoint)
        if not name:
            return '/'
        if name == 'vehicle_map':
            vid = kwargs.get('vehicle_id')
            return f"/vehicle/map?vehicle_id={vid}"
        if name == 'vehicle_history':
            vid = kwargs.get('vehicle_id')
            return f"/vehicle/history?vehicle_id={vid}"
        return reverse(name)

    @pass_context
    def csrf_field(ctx):
        try:
            request = ctx.get('request')
            token = get_token(request) if request else ''
            return mark_safe(f'<input type="hidden" name="csrfmiddlewaretoken" value="{token}">')
        except Exception:
            return ''

    env.globals.update(url_for=url_for, csrf_field=csrf_field)
    return env

