import os
from urllib.parse import urlparse
from pymongo import MongoClient
from django.conf import settings

_client = None


def get_mongo_client():
    global _client
    if _client is None:
        uri = getattr(settings, 'MONGO_URI', None) or os.getenv('MONGO_URI', 'mongodb://localhost:27017/gps_monitoring')
        _client = MongoClient(uri)
    return _client


def get_db():
    uri = getattr(settings, 'MONGO_URI', None) or os.getenv('MONGO_URI', 'mongodb://localhost:27017/gps_monitoring')
    parsed = urlparse(uri)
    dbname = (parsed.path or '/gps_monitoring').lstrip('/') or 'gps_monitoring'
    return get_mongo_client()[dbname]

