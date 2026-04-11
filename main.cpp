#include <curl/curl.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t totalSize = size * nmemb;
    std::string *response = static_cast<std::string *>(userp);
    response->append(static_cast<char *>(contents), totalSize);
    return totalSize;
}

std::string httpGet(const std::string &url) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("curl_easy_init failed");
    }

    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::string err = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        throw std::runtime_error("Request failed: " + err);
    }

    curl_easy_cleanup(curl);
    return response;
}

int main() {
    try {
        std::string adress = "192.168.11.21";

        std::string link_key = "http://" + adress + ":8080/api/info/list";
        std::string link_rsa = "http://" + adress + ":8080/api/rsa";

        std::string answer_key = httpGet(link_key);
        std::string answer_rsa = httpGet(link_rsa);

        auto key_json = nlohmann::json::parse(answer_key);
        auto rsa_json = nlohmann::json::parse(answer_rsa);

        std::cout << "--- /api/info/list ---\n";
        auto id = key_json["db"]["ownerId"];
        std::cout << "ownerId: " << id << "\n";
        std::cout << "RSA start: " << key_json["rsa"]["startDate"] << "\n";
        std::cout << "RSA expire: " << key_json["rsa"]["expireDate"] << "\n";
        std::cout << "GOST start: " << key_json["gost"]["startDate"] << "\n";
        std::cout << "GOST expire: " << key_json["gost"]["expireDate"] << "\n";

        // std::cout << "rsa_json:\n" << rsa_json.dump(2) << "\n";
        for (auto &row : rsa_json["rows"]) {
            if (id == row.at("Owner_ID")) {
                std::cout << "Fact_Address: " << row.at("Fact_Address") << "\n";
            }
        }

    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}