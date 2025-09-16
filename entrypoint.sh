#!/bin/sh
set -e

python manage.py collectstatic --noinput
python manage.py migrate --noinput

# Ensure default admin user exists
python - <<'PY'
import os
os.environ.setdefault('DJANGO_SETTINGS_MODULE', 'gps_site.settings')
import django
django.setup()
from django.contrib.auth import get_user_model

User = get_user_model()
username = os.getenv('DEFAULT_ADMIN_USER', 'admin')
password = os.getenv('DEFAULT_ADMIN_PASS', 'admin123')
email = os.getenv('DEFAULT_ADMIN_EMAIL', 'admin@example.com')

try:
    if not User.objects.filter(username=username).exists():
        User.objects.create_superuser(username=username, email=email, password=password)
        print(f"[init] Created default admin user: {username}")
    else:
        print(f"[init] Default admin user already exists: {username}")
except Exception as e:
    print(f"[init] Skipped creating default admin user due to error: {e}")
PY

exec daphne -b 0.0.0.0 -p 8001 gps_site.asgi:application
