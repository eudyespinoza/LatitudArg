from django.db import migrations


def add_missing_columns(apps, schema_editor):
    connection = schema_editor.connection
    cursor = connection.cursor()
    def has_column(table, col):
        cursor.execute(f"PRAGMA table_info({table})")
        return any(row[1] == col for row in cursor.fetchall())

    # Add columns to users if missing
    for col, sql_type, default in [
        ('is_active', 'INTEGER', '1'),
        ('is_staff', 'INTEGER', '0'),
        ('is_superuser', 'INTEGER', '0'),
        ('last_login', 'DATETIME', 'NULL'),
    ]:
        if not has_column('users', col):
            cursor.execute(f"ALTER TABLE users ADD COLUMN {col} {sql_type} DEFAULT {default}")


class Migration(migrations.Migration):
    dependencies = [
        ('gpsapp', '0001_initial'),
    ]

    operations = [
        migrations.RunPython(add_missing_columns, reverse_code=migrations.RunPython.noop),
    ]

