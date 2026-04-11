#pragma once
#include <nlohmann/json.hpp>
#define CPPHTTPLIB_THREAD_POOL_COUNT 4
#include <httplib.h>

class UTMDashboard {
public:
    UTMDashboard();
    ~UTMDashboard() = default;

    UTMDashboard(UTMDashboard const &) = delete;
    UTMDashboard(UTMDashboard &&) = delete;
    UTMDashboard &operator=(UTMDashboard const &) = delete;
    UTMDashboard &operator=(UTMDashboard &&) = delete;

    void run();

private:
    static std::time_t parse_date_time(const std::string &s);
    static std::string expire_class(const std::string &expireDate);
    static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);
    std::string http_get(std::string const &url);
    std::string get_detail_utm(std::string_view ip, std::string_view name);

    httplib::Server m_server;
    ::nlohmann::json m_utms;
};
