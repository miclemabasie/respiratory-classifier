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


# Update your PredictAudioAPIView in Django
import logging

logger = logging.getLogger(__name__)


# class PredictAudioAPIView(APIView):
#     def post(self, request, *args, **kwargs):
#         # Add request validation
#         if "wav_file" not in request.FILES:
#             return Response(
#                 {"error": "No audio file provided"}, status=status.HTTP_400_BAD_REQUEST
#             )

#         audio_file = request.FILES["wav_file"]

#         # Validate file type
#         if not audio_file.name.endswith(".wav"):
#             return Response(
#                 {"error": "Invalid file format. Only WAV files accepted"},
#                 status=status.HTTP_400_BAD_REQUEST,
#             )

#         # # Validate file size (max 2MB)
#         # if audio_file.size > 2 * 1024 * 1024:
#         #     return Response(
#         #         {"error": "File too large. Max size 2MB"},
#         #         status=status.HTTP_400_BAD_REQUEST,
#         #     )

#         try:
#             # Save temporarily
#             with open("temp.wav", "wb+") as temp_file:
#                 for chunk in audio_file.chunks():
#                     temp_file.write(chunk)

#             # Log file info
#             logger.info(
#                 f"Received WAV file: {audio_file.name}, Size: {audio_file.size} bytes"
#             )

#             # Call your pipeline
#             result = predict_respiratory_condition("temp.wav")

#             # Log prediction result
#             logger.info(f"Prediction result: {result}")

#             return Response(
#                 {"success": True, "prediction": result}, status=status.HTTP_200_OK
#             )

#         except Exception as e:
#             logger.error(f"Processing error: {str(e)}")
#             return Response(
#                 {"error": "Audio processing failed", "detail": str(e)},
#                 status=status.HTTP_500_INTERNAL_SERVER_ERROR,
#             )


from django.shortcuts import render
from rest_framework.views import APIView
from rest_framework.response import Response
from rest_framework import status
import os


# Create your views here.
class PredictAudioAPIView(APIView):
    """
    API endpoint to accept filename and return prediction result
    """

    def post(self, request, *args, **kwargs):
        # Get filename from request body
        filename = request.data.get("filename")

        if not filename:
            return Response(
                {"error": "No filename provided"}, status=status.HTTP_400_BAD_REQUEST
            )

        # Define path to your WAV files directory
        WAV_DIR = "./data"
        file_path = os.path.join(WAV_DIR, "122_2b1_Al_mc_LittC2SE.wav")
        print(file_path)
        if not os.path.exists(file_path):
            print("File not found on server")

        try:
            # Call your prediction pipeline
            result = predict_respiratory_condition("./122_2b1_Al_mc_LittC2SE.wav")

            # Format the response
            response_data = {
                "success": True,
                "prediction": result["prediction"],
                "probability": result["probability"],
            }

            return Response(response_data, status=status.HTTP_200_OK)

        except Exception as e:
            return Response(
                {"error": str(e)}, status=status.HTTP_500_INTERNAL_SERVER_ERROR
            )
