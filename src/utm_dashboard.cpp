#include "utm_dashboard.hpp"
#include <fmt/format.h>
#include <future>
#include <queue>
#include <syslog.h>

UTMDashboard::UTMDashboard(uint64_t port) : m_port{port} {
    openlog("UTMDashboard", LOG_PID | LOG_CONS, LOG_USER);
    setlogmask(LOG_UPTO(LOG_INFO));
    std::ifstream file("utms.json");
    if (!file.is_open()) {
        auto err = errno;
        syslog(LOG_ERR, "Не могу открыть список УТМ(utms.json): %s", strerror(err));
        throw std::runtime_error(::fmt::format("Не могу открыть список УТМ(utms.json): {}", strerror(err)));
    }

    file >> m_utms;
    syslog(LOG_INFO, "Загружен список УТМ из %lu шт.", m_utms.size());
    syslog(LOG_DEBUG, "УТМы:\n%s", m_utms.dump(2).c_str());
    file.close();
}
std::time_t UTMDashboard::parse_date_time(const std::string &s) {
    std::tm tm{};
    std::istringstream ss(s);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        return 0;
    }
    return std::mktime(&tm);
}
std::string UTMDashboard::expire_class(const std::string &expireDate) {
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

std::string UTMDashboard::get_detail_utm(std::string_view ip, std::string_view name) {
    std::string html;
    try {
        nlohmann::json key_json;
        nlohmann::json rsa_json;

        httplib::Client get_key(ip.data(), 8080);
        if (auto res = get_key.Get("/api/info/list")) {
            key_json = nlohmann::json::parse(res->body);
            syslog(LOG_DEBUG, "Получен ответ от '%s' -> '%s' по пути /api/info/list:\n%s", name.data(), ip.data(),
                   key_json.dump(2).c_str());
        } else {
            throw std::runtime_error(
                    ::fmt::format("Не удалось получить данные УТМ по адресу {}:8080/api/info/list. Причина: {}", ip,
                                  httplib::to_string(res.error())));
        }
        httplib::Client get_rsa(ip.data(), 8080);
        if (auto res = get_rsa.Get("/api/rsa")) {
            rsa_json = nlohmann::json::parse(res->body);
            syslog(LOG_DEBUG, "Получен ответ от '%s' -> '%s' по пути /api/rsa:\n%s", name.data(), ip.data(),
                   rsa_json.dump(2).c_str());
        } else {
            throw std::runtime_error(
                    ::fmt::format("Не удалось получить данные УТМ по адресу {}:8080/api/rsa. Причина: {}", ip,
                                  httplib::to_string(res.error())));
        }

        std::string link_app = fmt::format("http://{}:8080/app/", ip);


        std::string owner_id = key_json["db"]["ownerId"];
        std::string rsa_start = key_json["rsa"]["startDate"];
        std::string rsa_expire = key_json["rsa"]["expireDate"];
        std::string gost_start = key_json["gost"]["startDate"];
        std::string gost_expire = key_json["gost"]["expireDate"];
        std::string fact_address;

        std::string rsa_class = expire_class(rsa_expire);
        std::string gost_class = expire_class(gost_expire);

        syslog(LOG_DEBUG, "Пытаюсь получить фактический адрес по ID(%s)...", owner_id.c_str());
        for (auto const &row: rsa_json["rows"]) {
            if (row.contains("Owner_ID") && row["Owner_ID"] == owner_id) {
                syslog(LOG_DEBUG, "Найдена организация по ID(%s):\n%s", row["Owner_ID"].get<std::string>().c_str(),
                       row.dump(2).c_str());
                fact_address = row.at("Fact_Address");
                break;
            }
        }

        syslog(LOG_INFO,
               "Получены данные УТМ '%s' -> '%s': owner_id='%s', RSA='%s'..'%s', GOST='%s'..'%s', address='%s'",
               name.data(), ip.data(), owner_id.c_str(), rsa_start.c_str(), rsa_expire.c_str(), gost_start.c_str(),
               gost_expire.c_str(), fact_address.c_str());

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

        syslog(LOG_DEBUG, "Сформирован ответ для '%s' -> '%s': %s", name.data(), ip.data(), html.c_str());

        return html;
    } catch (const std::exception &ex) {
        std::string link_app = fmt::format("http://{}:8080/app/", ip);
        std::string name_link = fmt::format("<a href='{}' target='_blank'>{}</a>", link_app, name);
        html = ::fmt::format("<tr><td>{}</td><td class='danger' colspan='6'>{}</td></tr>", name_link, ex.what());
        syslog(LOG_ERR, "Ошибка при обработке '%s' -> '%s': %s", name.data(), ip.data(), ex.what());
        return html;
    }
}
void UTMDashboard::run() {
    m_server.Get("/", [this](const httplib::Request &, httplib::Response &res) {
        try {
            std::queue<std::future<std::string>> utms_detail;
            std::string html = "<html><head><meta charset='utf-8'><title>RAIPO UTM Dashboard</title>"
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

            for (const auto &utm: m_utms) {
                auto ip = utm.value("ip", "");
                auto name = utm.value("name", "");

                utms_detail.push(
                        std::async(std::launch::async, [this, ip, name]() { return get_detail_utm(ip, name); }));
            }

            while (!utms_detail.empty()) {
                html += utms_detail.front().get();
                utms_detail.pop();
            }

            html += "</table></body></html>";
            syslog(LOG_DEBUG, "Обработка запроса завершена, итоговый результат:\n%s", html.c_str());

            res.set_content(html, "text/html; charset=utf-8");
        } catch (const std::exception &ex) {
            res.status = 500;
            res.set_content(std::string("") + ex.what(), "text/plain; charset=utf-8");
        }
    });

    syslog(LOG_NOTICE, "Запущен сервер на http://127.0.0.1:%d", static_cast<int>(m_port));
    m_server.listen("0.0.0.0", static_cast<int>(m_port));
}
