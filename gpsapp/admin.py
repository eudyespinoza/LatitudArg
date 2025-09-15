from django.contrib import admin
from django.contrib.auth.admin import UserAdmin as BaseUserAdmin
from .models import User, Vehicle, LocationHistory


@admin.register(User)
class UserAdmin(BaseUserAdmin):
    fieldsets = (
        (None, {'fields': ('username', 'email', 'password')}),
        ('Info', {'fields': ('role', 'keyword')}),
        ('Permisos', {'fields': ('is_active', 'is_staff', 'is_superuser', 'groups', 'user_permissions')}),
    )
    add_fieldsets = ((None, {'classes': ('wide',), 'fields': ('username', 'email', 'password1', 'password2')}),)
    list_display = ('id', 'username', 'email', 'role', 'is_active')
    search_fields = ('username', 'email')
    ordering = ('id',)


@admin.register(Vehicle)
class VehicleAdmin(admin.ModelAdmin):
    list_display = ('id', 'name', 'patente', 'user', 'device_id', 'lat', 'lng', 'status', 'last_updated')
    search_fields = ('name', 'patente', 'device_id')
    list_filter = ('status',)


@admin.register(LocationHistory)
class LocationHistoryAdmin(admin.ModelAdmin):
    list_display = ('id', 'vehicle', 'lat', 'lng', 'speed', 'signal_quality', 'vehicle_on', 'timestamp')
    search_fields = ('vehicle__name', 'vehicle__patente')

