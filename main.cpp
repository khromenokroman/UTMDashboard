#include <curl/curl.h>
#include <fmt/format.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#define CPPHTTPLIB_THREAD_POOL_COUNT 4
#include <future>
#include <queue>


#include "httplib.h"

size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t totalSize = size * nmemb;
    std::string *response = static_cast<std::string *>(userp);
    response->append(static_cast<char *>(contents), totalSize);
    return totalSize;
}

std::string httpGet(std::string const &url) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("curl_easy_init failed");
    }

    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.data());
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

std::string get_detail_utm(std::string_view ip, std::string_view name) {
    std::string html;
    try {
        std::string link_key = ::fmt::format("http://{}:8080/api/info/list", ip);
        std::string link_rsa = fmt::format("http://{}:8080/api/rsa", ip);

        auto key_json = nlohmann::json::parse(httpGet(link_key));
        auto rsa_json = nlohmann::json::parse(httpGet(link_rsa));

        std::string owner_id = key_json["db"]["ownerId"];
        std::string rsa_start = key_json["rsa"]["startDate"];
        std::string rsa_expire = key_json["rsa"]["expireDate"];
        std::string gost_start = key_json["gost"]["startDate"];
        std::string gost_expire = key_json["gost"]["expireDate"];
        std::string fact_address;
        for (auto const &row: rsa_json["rows"]) {
            if (row["Owner_ID"] == owner_id) {
                fact_address = row.at("Fact_Address");
                break;
            }
        }
        html = ::fmt::format(
                "<tr><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{}</td></tr>",
                name, ip, owner_id, rsa_start, rsa_expire, gost_start, gost_expire, fact_address);
        return html;
    } catch (const std::exception &ex) {
        html = ::fmt::format("<tr><td>{}</td><td>{}</td><td colspan='6'>Error: {}</td></tr>", name, ip, ex.what());
        return html;
    }
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
    std::cout << "UTMs loaded: " << utms.size() << "\n";
    std::cout << "UTMs: " << utms.dump(2) << "\n";
    file.close();

    server.Get("/", [&utms](const httplib::Request &, httplib::Response &res) {
        try {
            std::queue<std::future<std::string>> utms_detail;
            std::string html = "<html><head><meta charset='utf-8'><title>UTM info</title>"
                   "<style>"
                   "body{font-family:Arial,sans-serif;margin:20px;}"
                   "table{border-collapse:collapse;width:100%;margin:0 auto;}"
                   "th,td{border:1px solid #ccc;padding:8px 12px;}"
                   "th{background:#f2f2f2;text-align:center;vertical-align:middle;}"
                   "td{text-align:left;vertical-align:top;}"
                   "</style>"
                   "</head><body>"
                   "<h1 style='text-align:center;'>Данные УТМ</h1>"
                   "<table>"
                   "<tr>"
                   "<th>Имя</th>"
                   "<th>IP</th>"
                   "<th>ID ключа</th>"
                   "<th>RSA записан</th>"
                   "<th>RSA истекает</th>"
                   "<th>GOST записан</th>"
                   "<th>GOST истекает</th>"
                   "<th>Фактический адрес (в ключе)</th>"
                   "</tr>";

            for (const auto &utm: utms) {
                utms_detail.push(std::async(std::launch::async, [&]() {
                    return get_detail_utm(utm.value("ip", ""), utm.value("name", ""));
                }));
            }

            while (!utms_detail.empty()) {
                html += utms_detail.front().get();
                utms_detail.pop();
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
