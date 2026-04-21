#include "utm_dashboard.hpp"
#include <fmt/format.h>
#include <future>
#include <queue>
#include <syslog.h>

UTMDashboard::UTMDashboard() {
    openlog("UTMDashboard", LOG_PID | LOG_CONS, LOG_USER);

    std::ifstream file_cfg(m_file_cfg.data());
    if (!file_cfg.is_open()) {
        auto err = errno;
        syslog(LOG_ERR, "Не могу открыть настройки(%s): %s", m_file_cfg.data(), strerror(err));
        throw std::runtime_error(::fmt::format("Не могу открыть настройки({}): {}", m_file_cfg, strerror(err)));
    }
    ::nlohmann::json cfg;
    file_cfg >> cfg;
    file_cfg.close();
    m_port = cfg.value("port", 8080);
    auto log_level = cfg.value("log_level", LOG_INFO);
    setlogmask(LOG_UPTO(log_level));

    std::ifstream file_utms(m_file_utms.data());
    if (!file_utms.is_open()) {
        auto err = errno;
        syslog(LOG_ERR, "Не могу открыть список УТМ(%s): %s", m_file_utms.data(), strerror(err));
        throw std::runtime_error(::fmt::format("Не могу открыть список УТМ({}): {}", m_file_utms, strerror(err)));
    }

    file_utms >> m_utms;
    syslog(LOG_INFO, "Загружен список УТМ из %lu шт.", m_utms.size());
    syslog(LOG_DEBUG, "УТМы:\n%s", m_utms.dump(2).c_str());
    file_utms.close();
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

std::string UTMDashboard::get_detail_utm(std::string ip, std::string name) {
    std::string html;
    try {
        nlohmann::json key_json;
        nlohmann::json org_json;

        httplib::Client get_key(ip.c_str(), 8080);
        if (auto res = get_key.Get("/api/info/list")) {
            key_json = nlohmann::json::parse(res->body);
            syslog(LOG_DEBUG, "Получен ответ от '%s' -> '%s' по пути /api/info/list:\n%s", name.c_str(), ip.c_str(),
                   key_json.dump(2).c_str());
        } else {
            throw std::runtime_error(
                    ::fmt::format("Не удалось получить данные УТМ по адресу {}:8080/api/info/list. Причина: {}", ip,
                                  httplib::to_string(res.error())));
        }

        std::string link_app = fmt::format("http://{}:8080/app/", ip);

        syslog(LOG_DEBUG, "Разбор данных...");
        std::string owner_id = key_json["db"]["ownerId"];
        syslog(LOG_DEBUG, "Получен owner_id");
        std::string rsa_start = key_json["rsa"]["startDate"];
        syslog(LOG_DEBUG, "Получен rsa_start");
        std::string rsa_expire = key_json["rsa"]["expireDate"];
        syslog(LOG_DEBUG, "Получен rsa_expire");
        std::string gost_start = key_json["gost"]["startDate"];
        syslog(LOG_DEBUG, "Получен gost_start");
        std::string gost_expire = key_json["gost"]["expireDate"];
        syslog(LOG_DEBUG, "Получен gost_expire");

        std::string rsa_class = expire_class(rsa_expire);
        std::string gost_class = expire_class(gost_expire);

        httplib::Client get_rsa(ip.c_str(), 8080);
        if (auto res = get_rsa.Get("/api/rsa")) {
            org_json = nlohmann::json::parse(res->body);
            syslog(LOG_DEBUG, "Получен ответ от '%s' -> '%s' по пути /api/rsa:\n%s", name.c_str(), ip.c_str(),
                   org_json.dump(2).c_str());
        } else {
            throw std::runtime_error(
                    ::fmt::format("Не удалось получить данные УТМ по адресу {}:8080/api/rsa. Причина: {}", ip,
                                  httplib::to_string(res.error())));
        }

        std::string fact_address;
        syslog(LOG_DEBUG, "Пытаюсь получить фактический адрес по ID(%s)...", owner_id.c_str());
        for (auto const &row: org_json["rows"]) {
            if (row.contains("Owner_ID") && row["Owner_ID"] == owner_id) {
                syslog(LOG_DEBUG, "Найдена организация по ID(%s):\n%s", row["Owner_ID"].get<std::string>().c_str(),
                       row.dump(2).c_str());
                fact_address = row.at("Fact_Address");
                break;
            }
        }

        syslog(LOG_INFO,
               "Получены данные УТМ '%s' -> '%s': owner_id='%s', RSA='%s'..'%s', GOST='%s'..'%s', address='%s'",
               name.c_str(), ip.c_str(), owner_id.c_str(), rsa_start.c_str(), rsa_expire.c_str(), gost_start.c_str(),
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

        syslog(LOG_DEBUG, "Сформирован ответ для '%s' -> '%s': %s", name.c_str(), ip.c_str(), html.c_str());

        return html;
    } catch (const std::exception &ex) {
        std::string link_app = fmt::format("http://{}:8080/app/", ip);
        std::string name_link = fmt::format("<a href='{}' target='_blank'>{}</a>", link_app, name);
        html = ::fmt::format("<tr><td>{}</td><td class='danger' colspan='6'>{}</td></tr>", name_link, ex.what());
        syslog(LOG_ERR, "Ошибка при обработке '%s' -> '%s': %s", name.c_str(), ip.c_str(), ex.what());
        return html;
    }
}
void UTMDashboard::run() {
    m_server.Get("/", [this](const httplib::Request &req, httplib::Response &res) {
        try {
            syslog(LOG_DEBUG, "Поступил запрос от %s:%d на %s:%d", req.remote_addr.c_str(), req.remote_port,
                   req.local_addr.c_str(), req.local_port);

            std::queue<std::future<std::string>> utms_detail;
            std::string html = R"html(
<!doctype html>
<html lang="ru">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RAIPO UTM Dashboard</title>
    <style>
        :root {
            --bg1: #e7edf4;
            --bg2: #d9e2ec;
            --bg3: #cfd9e6;
            --card: rgba(255, 255, 255, 0.72);
            --card-border: rgba(31, 41, 55, 0.12);
            --text: #1f2937;
            --muted: #5f6b7a;
            --accent: #1d4ed8;
            --accent2: #0f766e;
            --danger-bg: #f9d7d7;
            --warn-bg: #f6e7b6;
            --ok-bg: #d8eedf;
            --danger-text: #8f1d35;
            --warn-text: #8a5a00;
            --ok-text: #15603c;
            --shadow: 0 14px 34px rgba(15, 23, 42, 0.14);
        }

        * {
            box-sizing: border-box;
        }

        body {
            margin: 0;
            min-height: 100vh;
            font-family: Cambria, serif;
            color: var(--text);
            background:
                radial-gradient(circle at top left, rgba(29, 78, 216, 0.10), transparent 28%),
                radial-gradient(circle at top right, rgba(15, 118, 110, 0.09), transparent 26%),
                linear-gradient(135deg, var(--bg1), var(--bg2) 55%, var(--bg3));
            padding: 24px;
        }

        .wrapper {
            width: 100%;
            max-width: 1400px;
            margin: 0 auto;
            background: var(--card);
            backdrop-filter: blur(9px);
            border: 1px solid var(--card-border);
            border-radius: 28px;
            box-shadow: var(--shadow);
            padding: 34px;
        }

        h1 {
            margin: 0 0 8px;
            font-size: 42px;
            font-weight: 700;
            letter-spacing: 0.3px;
            text-align: center;
            color: #111827;
        }

        .subtitle {
            margin: 0 0 26px;
            font-size: 18px;
            color: var(--muted);
            text-align: center;
        }

        .table-wrap {
            overflow-x: auto;
            border-radius: 22px;
            border: 1px solid var(--card-border);
            box-shadow: 0 10px 24px rgba(15, 23, 42, 0.10);
            background: rgba(255, 255, 255, 0.55);
        }

        table {
            width: 100%;
            border-collapse: collapse;
            min-width: 1080px;
        }

        th, td {
            padding: 12px 14px;
            border-bottom: 1px solid rgba(15, 23, 42, 0.08);
            border-right: 1px solid rgba(15, 23, 42, 0.06);
            vertical-align: top;
            text-align: left;
        }

        th:last-child, td:last-child {
            border-right: none;
        }

        th {
            position: sticky;
            top: 0;
            z-index: 1;
            background: #e8eef5;
            text-align: center;
            font-size: 16px;
            letter-spacing: 0.2px;
            color: #0f172a;
        }

        tbody tr td {
            background: rgba(255, 255, 255, 0.58);
        }

        tr:hover td {
            background: rgba(241, 247, 255, 0.95);
        }

        td {
            font-size: 15px;
        }

        td.danger {
            background: var(--danger-bg);
            color: var(--danger-text);
            font-weight: 700;
        }

        td.warn {
            background: var(--warn-bg);
            color: var(--warn-text);
            font-weight: 700;
        }

        td.ok {
            background: var(--ok-bg);
            color: var(--ok-text);
            font-weight: 700;
        }

        a {
            color: var(--accent);
            text-decoration: none;
            font-weight: 700;
        }

        a:hover {
            color: #1e40af;
            text-decoration: underline;
        }

        .footer {
            margin-top: 18px;
            font-size: 14px;
            color: var(--muted);
            text-align: center;
        }
    </style>
</head>
<body>
    <div class="wrapper">
        <h1>Данные УТМ</h1>
        <p class="subtitle">Список устройств, сроки сертификатов и фактические адреса</p>

        <div class="table-wrap">
            <table>
                <tr>
                    <th>Имя</th>
                    <th>ID ключа</th>
                    <th>RSA записан</th>
                    <th>RSA истекает</th>
                    <th>GOST записан</th>
                    <th>GOST истекает</th>
                    <th>Фактический адрес (в ключе)</th>
                </tr>
)html";

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

            html += R"html(
            </table>
        </div>

        <div class="footer">RAIPO UTM Dashboard</div>
    </div>
</body>
</html>
)html";
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
