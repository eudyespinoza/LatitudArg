from django.db import migrations, models
import django.db.models.deletion


class Migration(migrations.Migration):
    initial = True

    dependencies = [
        ('auth', '0012_alter_user_first_name_max_length'),
    ]

    operations = [
        migrations.CreateModel(
            name='User',
            fields=[
                ('id', models.AutoField(primary_key=True, serialize=False)),
                ('password', models.CharField(max_length=255)),
                ('last_login', models.DateTimeField(blank=True, null=True)),
                ('is_superuser', models.BooleanField(default=False)),
                ('username', models.CharField(max_length=150, unique=True)),
                ('email', models.EmailField(max_length=254, unique=True)),
                ('role', models.CharField(default='user', max_length=32)),
                ('keyword', models.CharField(blank=True, default='', max_length=255)),
                ('is_active', models.BooleanField(default=True)),
                ('is_staff', models.BooleanField(default=False)),
                ('groups', models.ManyToManyField(blank=True, related_name='user_set', related_query_name='user', to='auth.group')),
                ('user_permissions', models.ManyToManyField(blank=True, related_name='user_set', related_query_name='user', to='auth.permission')),
            ],
            options={'db_table': 'users'},
        ),
        migrations.CreateModel(
            name='Vehicle',
            fields=[
                ('id', models.AutoField(primary_key=True, serialize=False)),
                ('name', models.CharField(max_length=255)),
                ('type', models.CharField(max_length=100)),
                ('patente', models.CharField(max_length=100)),
                ('lat', models.FloatField(default=-34.6037)),
                ('lng', models.FloatField(default=-58.3816)),
                ('status', models.CharField(default='active', max_length=32)),
                ('device_id', models.CharField(blank=True, max_length=255, null=True, unique=True)),
                ('signal_quality', models.IntegerField(default=0)),
                ('vehicle_on', models.BooleanField(default=False)),
                ('shutdown', models.BooleanField(default=False)),
                ('transmit_audio', models.BooleanField(default=False)),
                ('speed', models.FloatField(default=0.0)),
                ('last_updated', models.CharField(blank=True, max_length=32, null=True)),
                ('user', models.ForeignKey(db_column='user_id', on_delete=django.db.models.deletion.CASCADE, to='gpsapp.user')),
            ],
            options={'db_table': 'vehicles'},
        ),
        migrations.CreateModel(
            name='LocationHistory',
            fields=[
                ('id', models.AutoField(primary_key=True, serialize=False)),
                ('lat', models.FloatField()),
                ('lng', models.FloatField()),
                ('speed', models.FloatField()),
                ('signal_quality', models.IntegerField()),
                ('vehicle_on', models.BooleanField()),
                ('timestamp', models.DateTimeField(auto_now_add=True)),
                ('vehicle', models.ForeignKey(db_column='vehicle_id', on_delete=django.db.models.deletion.CASCADE, related_name='history', to='gpsapp.vehicle')),
            ],
            options={'db_table': 'location_history'},
        ),
    ]

