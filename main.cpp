#include <curl/curl.h>
#include <fmt/format.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#define CPPHTTPLIB_THREAD_POOL_COUNT 4
#include "httplib.h"

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
    httplib::Server server;
    std::ifstream file("utms.json");
    if (!file.is_open()) {
        auto err = errno;
        throw std::runtime_error(::fmt::format("Failed to open utms.json: {}", strerror(err)));
    }
    ::nlohmann::json utms;
    file >> utms;
    file.close();

    server.Get("/", [&utms](const httplib::Request &, httplib::Response &res) {
        try {
            std::string html =
                "<html><head><meta charset='utf-8'><title>UTM info</title>"
                "<style>"
                "body{font-family:Arial,sans-serif;margin:20px;}"
                "table{border-collapse:collapse;width:100%;}"
                "th,td{border:1px solid #ccc;padding:8px 12px;text-align:left;vertical-align:top;}"
                "th{background:#f2f2f2;}"
                "</style>"
                "</head><body>"
                "<h1>Данные УТМ</h1>"
                "<table>"
                "<tr>"
                "<th>Name</th>"
                "<th>IP</th>"
                "<th>Owner ID</th>"
                "<th>RSA start</th>"
                "<th>RSA expire</th>"
                "<th>GOST start</th>"
                "<th>GOST expire</th>"
                "<th>Fact Address</th>"
                "</tr>";

            for (const auto& utm : utms) {
                std::string name = utm.value("name", "");
                std::string adress = utm.value("ip", "");

                try {
                    std::string link_key = "http://" + adress + ":8080/api/info/list";
                    std::string link_rsa = "http://" + adress + ":8080/api/rsa";

                    auto key_json = nlohmann::json::parse(httpGet(link_key));
                    auto rsa_json = nlohmann::json::parse(httpGet(link_rsa));

                    std::string ownerId = key_json["db"]["ownerId"];
                    std::string rsaStart = key_json["rsa"]["startDate"];
                    std::string rsaExpire = key_json["rsa"]["expireDate"];
                    std::string gostStart = key_json["gost"]["startDate"];
                    std::string gostExpire = key_json["gost"]["expireDate"];

                    std::string factAddress = "Не найдено";
                    for (const auto& row : rsa_json["rows"]) {
                        if (row.contains("Owner_ID") && row["Owner_ID"] == ownerId) {
                            factAddress = row.value("Fact_Address", "Не найдено");
                            break;
                        }
                    }

                    html +=
                        "<tr>"
                        "<td>" + name + "</td>"
                        "<td>" + adress + "</td>"
                        "<td>" + ownerId + "</td>"
                        "<td>" + rsaStart + "</td>"
                        "<td>" + rsaExpire + "</td>"
                        "<td>" + gostStart + "</td>"
                        "<td>" + gostExpire + "</td>"
                        "<td>" + factAddress + "</td>"
                        "</tr>";
                } catch (const std::exception& ex) {
                    html +=
                        "<tr>"
                        "<td>" + name + "</td>"
                        "<td>" + adress + "</td>"
                        "<td colspan='6'>Error: " + std::string(ex.what()) + "</td>"
                        "</tr>";
                }
            }

            html += "</table></body></html>";

            res.set_content(html, "text/html; charset=utf-8");
        } catch (const std::exception &ex) {
            res.status = 500;
            res.set_content(std::string("Error: ") + ex.what(), "text/plain; charset=utf-8");
        }
    });

    std::cout << "Server started: http://127.0.0.1:8081/\n";
    server.listen("0.0.0.0", 8081);
    return 0;
}
