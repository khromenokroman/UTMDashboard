#include <curl/curl.h>
#include <fmt/format.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#define CPPHTTPLIB_THREAD_POOL_COUNT 4
#include <ctime>
#include <future>
#include <iomanip>
#include <queue>
#include <sstream>

#include "httplib.h"


static std::time_t parse_date_time(const std::string &s) {
    std::tm tm{};
    std::istringstream ss(s);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        return 0;
    }
    return std::mktime(&tm);
}

static std::string expire_class(const std::string &expireDate) {
    std::time_t t = parse_date_time(expireDate);
    if (t == 0) {
        return "";
    }

    std::time_t now = std::time(nullptr);
    double daysLeft = std::difftime(t, now) / (60.0 * 60.0 * 24.0);

    if (daysLeft <= 30.0) {
        return "danger";
    }
    if (daysLeft <= 60.0) {
        return "warn";
    }
    return "ok";
}

size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t totalSize = size * nmemb;
    std::string *response = static_cast<std::string *>(userp);
    response->append(static_cast<char *>(contents), totalSize);
    return totalSize;
}

std::string httpGet(std::string const &url) {
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), &curl_easy_cleanup);
    if (!curl) {
        throw std::runtime_error("curl_easy_init failed");
    }

    std::string response;

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl.get());
    if (res != CURLE_OK) {
        std::string err = curl_easy_strerror(res);
        throw std::runtime_error("Request failed: " + err);
    }

    return response;
}

std::string get_detail_utm(std::string_view ip, std::string_view name) {
    std::string html;
    try {
        std::string link_key = ::fmt::format("http://{}:8080/api/info/list", ip);
        std::string link_rsa = fmt::format("http://{}:8080/api/rsa", ip);
        std::string link_app = fmt::format("http://{}:8080/app/", ip);

        auto key_json = nlohmann::json::parse(httpGet(link_key));
        auto rsa_json = nlohmann::json::parse(httpGet(link_rsa));

        std::string owner_id = key_json["db"]["ownerId"];
        std::string rsa_start = key_json["rsa"]["startDate"];
        std::string rsa_expire = key_json["rsa"]["expireDate"];
        std::string gost_start = key_json["gost"]["startDate"];
        std::string gost_expire = key_json["gost"]["expireDate"];
        std::string fact_address;

        std::string rsa_class = expire_class(rsa_expire);
        std::string gost_class = expire_class(gost_expire);

        for (auto const &row: rsa_json["rows"]) {
            if (row.contains("Owner_ID") && row["Owner_ID"] == owner_id) {
                fact_address = row.at("Fact_Address");
                break;
            }
        }

        std::string name_link = fmt::format("<a href='{}' target='_blank'>{}</a>", link_app, name);

        html = ::fmt::format("<tr>"
                             "<td>{}</td>"
                             "<td>{}</td>"
                             "<td>{}</td>"
                             "<td class='{}'>{}</td>"
                             "<td>{}</td>"
                             "<td class='{}'>{}</td>"
                             "<td>{}</td>"
                             "</tr>",
                             name_link, owner_id, rsa_start, rsa_class, rsa_expire, gost_start, gost_class, gost_expire,
                             fact_address);

        return html;
    } catch (const std::exception &ex) {
        html = ::fmt::format("<tr><td>{}</td><td class='danger' colspan='6'>Error: {}</td></tr>", name, ex.what());
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
                               "td.danger{background-color:#ffcccc;font-weight:bold;}"
                               "td.warn{background-color:#fff3cd;}"
                               "td.ok{background-color:#d4edda;}"
                               "</style>"
                               "</head><body>"
                               "<h1 style='text-align:center;'>Данные УТМ</h1>"
                               "<table>"
                               "<tr>"
                               "<th>Имя</th>"
                               "<th>ID ключа</th>"
                               "<th>RSA записан</th>"
                               "<th>RSA истекает</th>"
                               "<th>GOST записан</th>"
                               "<th>GOST истекает</th>"
                               "<th>Фактический адрес (в ключе)</th>"
                               "</tr>";

            for (const auto &utm: utms) {
                auto ip = utm.value("ip", "");
                auto name = utm.value("name", "");

                utms_detail.push(std::async(std::launch::async, [ip, name]() { return get_detail_utm(ip, name); }));
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
