from rest_framework import serializers


class AudioUploadSerializer(serializers.Serializer):
    wav_file = serializers.FileField()

    def validate_wav_file(self, value):
        if not value.name.endswith(".wav"):
            raise serializers.ValidationError("Only .wav files are supported.")
        return value
