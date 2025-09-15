from django.db import models
from django.contrib.auth.models import AbstractBaseUser, PermissionsMixin, BaseUserManager
import bcrypt


class UserManager(BaseUserManager):
    def create_user(self, username, email, password=None, role='user', keyword=''):
        if not username:
            raise ValueError('El nombre de usuario es obligatorio')
        if not email:
            raise ValueError('El email es obligatorio')
        email = self.normalize_email(email)
        user = self.model(username=username, email=email, role=role, keyword=keyword)
        if password:
            user.set_password(password)
        else:
            # Establecer una contraseña unusable si no se provee
            user.set_unusable_password()
        user.save(using=self._db)
        return user

    def create_superuser(self, username, email, password):
        user = self.create_user(username=username, email=email, password=password, role='admin')
        user.is_staff = True
        user.is_superuser = True
        user.save(using=self._db)
        return user


class User(AbstractBaseUser, PermissionsMixin):
    id = models.AutoField(primary_key=True)
    username = models.CharField(max_length=150, unique=True)
    email = models.EmailField(unique=True)
    password = models.CharField(max_length=255)
    role = models.CharField(max_length=32, default='user')
    keyword = models.CharField(max_length=255, blank=True, default='')

    # Campos requeridos por Django admin
    is_active = models.BooleanField(default=True)
    is_staff = models.BooleanField(default=False)

    USERNAME_FIELD = 'username'
    REQUIRED_FIELDS = ['email']

    objects = UserManager()

    class Meta:
        db_table = 'users'

    def __str__(self):
        return self.username

    # Compatibilidad con hashes bcrypt "puros" heredados (sin prefijo Django)
    def check_password(self, raw_password):
        from django.contrib.auth.hashers import check_password as dj_check_password
        if dj_check_password(raw_password, self.password):
            return True
        try:
            if self.password and self.password.startswith('$2'):
                ok = bcrypt.checkpw(raw_password.encode('utf-8'), self.password.encode('utf-8'))
                if ok:
                    # actualizar al formato Django
                    self.set_password(raw_password)
                    self.save(update_fields=['password'])
                    return True
        except Exception:
            pass
        return False


class Vehicle(models.Model):
    id = models.AutoField(primary_key=True)
    user = models.ForeignKey(User, on_delete=models.CASCADE, db_column='user_id')
    name = models.CharField(max_length=255)
    type = models.CharField(max_length=100)
    patente = models.CharField(max_length=100)
    lat = models.FloatField(default=-34.6037)
    lng = models.FloatField(default=-58.3816)
    status = models.CharField(max_length=32, default='active')
    device_id = models.CharField(max_length=255, unique=True, null=True, blank=True)
    signal_quality = models.IntegerField(default=0)
    vehicle_on = models.BooleanField(default=False)
    shutdown = models.BooleanField(default=False)
    transmit_audio = models.BooleanField(default=False)
    speed = models.FloatField(default=0.0)
    last_updated = models.CharField(max_length=32, null=True, blank=True)

    class Meta:
        db_table = 'vehicles'

    def __str__(self):
        return f"{self.name} ({self.patente})"


class LocationHistory(models.Model):
    id = models.AutoField(primary_key=True)
    vehicle = models.ForeignKey(Vehicle, on_delete=models.CASCADE, db_column='vehicle_id', related_name='history')
    lat = models.FloatField()
    lng = models.FloatField()
    speed = models.FloatField()
    signal_quality = models.IntegerField()
    vehicle_on = models.BooleanField()
    timestamp = models.DateTimeField(auto_now_add=True)

    class Meta:
        db_table = 'location_history'
