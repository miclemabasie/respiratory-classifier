#include <iostream>
#include <fstream>
#include <curl/curl.h>

int main()
{
    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (curl)
    {
        // ✅ Your Django endpoint
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8000/core/api/predict/");

        struct curl_httppost *form = NULL;
        struct curl_httppost *last = NULL;

        // ✅ Add the file field
        curl_formadd(&form,
                     &last,
                     CURLFORM_COPYNAME, "wav_file",
                     CURLFORM_FILE, "./101_1b1_Al_sc_Meditron.wav",
                     CURLFORM_CONTENTTYPE, "audio/wav",
                     CURLFORM_END);

        curl_easy_setopt(curl, CURLOPT_HTTPPOST, form);

        // ✅ Print response to stdout
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, stdout);

        // ✅ Perform the request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));

        // ✅ Clean up
        curl_easy_cleanup(curl);
        curl_formfree(form);
    }

    curl_global_cleanup();

    return 0;
}
