from django.urls import path
from . import views

urlpatterns = [
    path('', views.index, name='index'),
    path('login', views.login_view, name='login'),
    path('register', views.contact_view, name='register'),
    path('contact', views.contact_view, name='contact'),
    path('dashboard', views.dashboard, name='dashboard'),
    path('admin-panel', views.admin_panel, name='admin_panel'),
    path('profile', views.profile, name='profile'),
    path('logout', views.logout_view, name='logout'),

    path('delete_user', views.delete_user, name='delete_user'),
    path('update_user', views.update_user, name='update_user'),
    path('delete_vehicle', views.delete_vehicle, name='delete_vehicle'),
    path('update_vehicle', views.update_vehicle, name='update_vehicle'),
    path('vehicle/map', views.vehicle_map, name='vehicle_map'),
    path('vehicle/history', views.vehicle_history, name='vehicle_history'),

    path('api/update_location', views.api_update_location, name='api_update_location'),
    path('api/vehicle/<int:vehicle_id>/history', views.api_vehicle_history, name='api_vehicle_history'),
    path('api/vehicle/<int:vehicle_id>/shutdown', views.api_shutdown_vehicle, name='api_shutdown_vehicle'),
    path('api/vehicle/<int:vehicle_id>/audio', views.api_toggle_audio, name='api_toggle_audio'),
]
