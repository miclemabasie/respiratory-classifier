import requests

# ✅ Your Django API endpoint
url = "http://localhost:8000/core/api/predict/"

# ✅ Open the file in binary mode
with open("./101_1b1_Al_sc_Meditron.wav", "rb") as f:
    files = {"wav_file": ("101_1b1_Al_sc_Meditron.wav", f, "audio/wav")}
    response = requests.post(url, files=files)

print(response.status_code)
print(response.json())
