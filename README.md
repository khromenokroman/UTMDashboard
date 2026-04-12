# UTM Dashboard

Веб-панель для просмотра данных по УТМ:
- имя и адрес УТМ
- идентификатор ключа
- период действия RSA
- период действия GOST
- фактический адрес из ключа

Приложение поднимает локальный HTTP-сервер и формирует HTML-таблицу по списку УТМ из файла `configuration/utms.json`.

## Возможности

- чтение списка УТМ из JSON
- параллельный опрос нескольких УТМ
- получение данных через API:
  - `/api/info/list`
  - `/api/rsa`
- подсветка сроков действия сертификатов
- вывод в браузер в виде таблицы
- логирование в syslog
- сборка Debian-пакета через CPack

## Требования

### Для сборки
- CMake 3.26+
- C++20
- `fmt`
- `cpp-httplib`
- `nlohmann-json`

````bash
apt install -y build-essential cmake libfmt-dev nlohmann-json3-dev dpkg-dev libcpp-httplib-dev
````

## Сборка из исходников
```bash 
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
cmake --install .
```

## Сборка DEB

Пакет собирается через CPack:
```bash 
cd build cpack -G DEB
apt install -y ./utm-dashboard_<версия>_amd64.deb
```

После установки будут размещены:

- бинарник: `/usr/bin/utm-dashboard`
- конфиг: `/etc/utms.json`
- unit-файл systemd: `/usr/lib/systemd/system/utm-dashboard.service`


## Запуск

После установки сервис можно запускать так:
```bash 
systemctl daemon-reload
systemctl enable utm-dashboard
systemctl start utm-dashboard
```
Проверка статуса:

```bash 
systemctl status utm-dashboard
```
По умолчанию сервер доступен на: http://localhost:8080

## Логирование

Приложение пишет сообщения в `syslog`.

Используются уровни:
- `LOG_ERR`
- `LOG_INFO`
- `LOG_NOTICE`
- `LOG_DEBUG`

Уровень фильтрации можно настраивать в коде через `setlogmask()`.
