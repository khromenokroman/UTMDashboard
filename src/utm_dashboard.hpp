#pragma once
#include <nlohmann/json.hpp>
#include <httplib.h>

class UTMDashboard {
public:
    UTMDashboard(uint64_t port);
    ~UTMDashboard() = default;

    UTMDashboard(UTMDashboard const &) = delete;
    UTMDashboard(UTMDashboard &&) = delete;
    UTMDashboard &operator=(UTMDashboard const &) = delete;
    UTMDashboard &operator=(UTMDashboard &&) = delete;

    void run();

private:
    static std::time_t parse_date_time(const std::string &s);
    static std::string expire_class(const std::string &expireDate);
    std::string get_detail_utm(std::string_view ip, std::string_view name);

    httplib::Server m_server; // 752
    ::nlohmann::json m_utms; // 16
    uint64_t m_port;
};
