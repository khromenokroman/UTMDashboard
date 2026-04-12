#include "utm_dashboard.hpp"


int main() {
    try {
        UTMDashboard dashboard;
        dashboard.run();
        return EXIT_SUCCESS;
    } catch (std::exception const &ex) {
        std::cerr << "Ошибка запуска приложения: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
