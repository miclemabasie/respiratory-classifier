from django.shortcuts import render
from .utils import predict_respiratory_condition
from .serializers import AudioUploadSerializer
from rest_framework.views import APIView
from rest_framework.response import Response
from rest_framework import status


# Create your views here.
def predict(request):
    template_name = "core/predict.html"
    context = {}
    """Predict respiratory condition based on audio input."""
    if request.method == "POST":
        audio_file = request.FILES.get("wav_file")
        if audio_file:
            # Save the uploaded file temporarily
            with open("temp.wav", "wb+") as temp_file:
                for chunk in audio_file.chunks():
                    temp_file.write(chunk)

            # Predict the respiratory condition
            prediction = predict_respiratory_condition("temp.wav")
            context = {"prediction": prediction}
        else:
            context = {"error": "No audio file provided."}
        return render(request, template_name, context)

    return render(request, template_name, context)


from .serializers import AudioUploadSerializer
from .utils import predict_respiratory_condition


class PredictAudioAPIView(APIView):
    """
    API endpoint to accept a WAV file upload and return prediction result.
    """

    def post(self, request, *args, **kwargs):
        serializer = AudioUploadSerializer(data=request.data)

        if serializer.is_valid():
            audio_file = serializer.validated_data["wav_file"]

            # Save temporarily
            with open("temp.wav", "wb+") as temp_file:
                for chunk in audio_file.chunks():
                    temp_file.write(chunk)

            # Call your pipeline
            result = predict_respiratory_condition("temp.wav")

            return Response(
                {"success": True, "prediction": result}, status=status.HTTP_200_OK
            )

        return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)

    def get(self, request, *args, **kwargs):
        return Response(
            {"message": "Use POST method to upload a WAV file for prediction."},
            status=status.HTTP_200_OK,
        )
