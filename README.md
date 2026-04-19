# Geomagnetic & Weather Telegram Bot (Belarus) 🇧🇾

C++ Telegram bot for monitoring geomagnetic activity (Kp-index) and weather in Belarus.

## Features

- **Geomagnetic monitoring**: Real-time Kp-index + 3-day forecast from NOAA
- **Weather**: Any city/village in Belarus (temperature, humidity, wind, feels like)
- **3 languages**: 🇷🇺 Русский / 🇧🇾 Беларуская / 🇬🇧 English
- **Auto alerts**: Morning report at 9:00 + storm warnings at Kp ≥ 5.0
- **Persistent storage**: Users, cities, language, notification settings

## Tech Stack

| Component | Technology |
|-----------|------------|
| Language | C++17 |
| HTTP | CPR (libcurl) |
| JSON | nlohmann/json |
| Threading | Pthreads |
| Weather API | OpenWeatherMap |
| Geomagnetic API | NOAA SWPC |

## Installation

### Dependencies
```bash
sudo apt-get install libcurl4-openssl-dev libssl-dev


### Compile
```bash
g++ -std=c++17 bot.cpp -o bot -lcpr -lcurl -lssl -lcrypto -pthread

### Run
```bash
export TG_BOT_TOKEN="YOUR_BOT_TOKEN"
./bot


### systemd Service

Create service file:
```bash
sudo nano /etc/systemd/system/geobot.service


[Unit]
Description=Geomagnetic Bot
After=network.target

[Service]
Type=simple
User=your_user
WorkingDirectory=/path/to/bot
Environment="TG_BOT_TOKEN=YOUR_TOKEN"
ExecStart=/path/to/bot/bot
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target


###Enable end Start:

```bash
sudo systemctl daemon-reload
sudo systemctl enable geobot.service
sudo systemctl start geobot.service




### Telegram
Bot: @geomagnetic_belarus_bot
