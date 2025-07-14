from django.urls import path
from .views import *


urlpatterns = [
    path(
        "predict/",
        predict,
        name="predict_respiratory_condition",
    ),
    path("api/predict/", PredictAudioAPIView.as_view(), name="api_predict"),
]
