import os
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent

SECRET_KEY = os.getenv('SECRET_KEY', 'replace-me-in-prod')


def _split_csv(value: str):
    return [item.strip() for item in value.split(',') if item.strip()]

DEBUG = os.getenv('DEBUG', '1') == '1'
DEFAULT_ALLOWED_HOSTS = [
    'localhost',
    '127.0.0.1',
    'latitudarg.com',
    'www.latitudarg.com',
    'latitudarg.com.ar',
    'www.latitudarg.com.ar',
]

_env_allowed_hosts = os.getenv('ALLOWED_HOSTS')
allowed_hosts_source = DEFAULT_ALLOWED_HOSTS.copy()
if _env_allowed_hosts:
    allowed_hosts_source += _split_csv(_env_allowed_hosts)
ALLOWED_HOSTS = list(dict.fromkeys(allowed_hosts_source))

DEFAULT_CSRF_TRUSTED_ORIGINS = [
    f'{scheme}://{domain}'
    for domain in [
        'latitudarg.com',
        'www.latitudarg.com',
        'latitudarg.com.ar',
        'www.latitudarg.com.ar',
    ]
    for scheme in ('https', 'http')
]

_env_csrf_trusted = os.getenv('CSRF_TRUSTED_ORIGINS')
csrf_trusted_source = DEFAULT_CSRF_TRUSTED_ORIGINS.copy()
if _env_csrf_trusted:
    csrf_trusted_source += _split_csv(_env_csrf_trusted)
CSRF_TRUSTED_ORIGINS = list(dict.fromkeys(csrf_trusted_source))

CONTACT_EMAIL = os.getenv('CONTACT_EMAIL', 'contacto@latitudarg.com')

INSTALLED_APPS = [
    'django.contrib.admin',
    'django.contrib.auth',
    'django.contrib.contenttypes',
    'django.contrib.sessions',
    'django.contrib.messages',
    'django.contrib.staticfiles',
    'channels',
    'gpsapp',
]

MIDDLEWARE = [
    'django.middleware.security.SecurityMiddleware',
    'whitenoise.middleware.WhiteNoiseMiddleware',
    'django.contrib.sessions.middleware.SessionMiddleware',
    'django.middleware.common.CommonMiddleware',
    'django.middleware.csrf.CsrfViewMiddleware',
    'django.contrib.auth.middleware.AuthenticationMiddleware',
    'django.contrib.messages.middleware.MessageMiddleware',
    'django.middleware.clickjacking.XFrameOptionsMiddleware',
]

ROOT_URLCONF = 'gps_site.urls'

TEMPLATES = [
    {
        'BACKEND': 'django.template.backends.jinja2.Jinja2',
        'DIRS': [str(BASE_DIR / 'app' / 'templates')],
        'APP_DIRS': False,
        'OPTIONS': {
            'environment': 'gps_site.jinja2_env.environment',
            'context_processors': [
                'django.template.context_processors.debug',
                'django.template.context_processors.request',
                'django.contrib.auth.context_processors.auth',
                'django.contrib.messages.context_processors.messages',
                'django.template.context_processors.csrf',
            ],
        },
    },
    {
        'BACKEND': 'django.template.backends.django.DjangoTemplates',
        'DIRS': [],
        'APP_DIRS': True,
        'OPTIONS': {
            'context_processors': [
                'django.template.context_processors.debug',
                'django.template.context_processors.request',
                'django.contrib.auth.context_processors.auth',
                'django.contrib.messages.context_processors.messages',
                'django.template.context_processors.csrf',
            ],
        },
    },
]

WSGI_APPLICATION = 'gps_site.wsgi.application'
ASGI_APPLICATION = 'gps_site.asgi.application'

DATABASES = {
    'default': {
        'ENGINE': 'django.db.backends.sqlite3',
        'NAME': os.getenv('SQLITE_DB_PATH', str(BASE_DIR / 'gps_monitoring.db')),
    }
}

AUTH_USER_MODEL = 'gpsapp.User'

AUTH_PASSWORD_VALIDATORS = [
    {'NAME': 'django.contrib.auth.password_validation.UserAttributeSimilarityValidator'},
    {'NAME': 'django.contrib.auth.password_validation.MinimumLengthValidator', 'OPTIONS': {'min_length': 6}},
]

PASSWORD_HASHERS = [
    'django.contrib.auth.hashers.BCryptPasswordHasher',
    'django.contrib.auth.hashers.BCryptSHA256PasswordHasher',
    'django.contrib.auth.hashers.PBKDF2PasswordHasher',
]

LANGUAGE_CODE = 'es-ar'
TIME_ZONE = 'America/Argentina/Buenos_Aires'
USE_I18N = True
USE_TZ = True

STATIC_URL = '/static/'
STATICFILES_DIRS = [
    str(BASE_DIR / 'app' / 'static'),
]
STATIC_ROOT = str(BASE_DIR / 'staticfiles')
STATICFILES_STORAGE = 'whitenoise.storage.CompressedManifestStaticFilesStorage'

DEFAULT_AUTO_FIELD = 'django.db.models.AutoField'

# Channels (Redis) config via env, default to in-memory for dev
REDIS_URL = os.getenv('REDIS_URL')
if REDIS_URL:
    CHANNEL_LAYERS = {
        'default': {
            'BACKEND': 'channels_redis.core.RedisChannelLayer',
            'CONFIG': {'hosts': [REDIS_URL]},
        }
    }
else:
    CHANNEL_LAYERS = {
        'default': {
            'BACKEND': 'channels.layers.InMemoryChannelLayer'
        }
    }

MONGO_URI = os.getenv('MONGO_URI', 'mongodb://mongodb:27017/gps_monitoring')
