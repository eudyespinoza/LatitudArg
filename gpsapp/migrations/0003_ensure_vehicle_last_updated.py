from django.db import migrations


def ensure_last_updated(apps, schema_editor):
    connection = schema_editor.connection
    cursor = connection.cursor()
    cursor.execute("PRAGMA table_info(vehicles)")
    cols = [row[1] for row in cursor.fetchall()]
    if 'last_updated' not in cols:
        cursor.execute("ALTER TABLE vehicles ADD COLUMN last_updated TEXT")


class Migration(migrations.Migration):
    dependencies = [
        ('gpsapp', '0002_add_missing_user_cols'),
    ]

    operations = [
        migrations.RunPython(ensure_last_updated, reverse_code=migrations.RunPython.noop),
    ]

