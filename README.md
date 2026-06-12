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
| Build | Make |
| Screens | HTML/CSS templates + wkhtmltoimage |
| Weather API | OpenWeatherMap |
| Geomagnetic API | NOAA SWPC |

## Installation

### Dependencies
```bash
sudo apt-get update
sudo apt-get install build-essential make libcurl4-openssl-dev libssl-dev libcpr-dev nlohmann-json3-dev wkhtmltopdf
```

`wkhtmltoimage` is used to render Telegram live screens as JPEG images. If it is missing, the bot falls back to text messages.

If your distribution does not provide `libcpr-dev`, install CPR from your package manager, vcpkg, or the upstream CPR project, then keep the linker flags from the `Makefile`.

### Compile
```bash
make
```

### Self-test
```bash
make test
```

### Screen Templates

Telegram images are rendered from editable templates:

- `templates/screen.html` - outer HTML shell
- `templates/screen.css` - visual style, layout, colors, typography

The bot reads these files every time it renders a new screen. CSS/HTML changes do not require recompiling C++; only restart or trigger a new screen render in Telegram.

### Run
Create `.env` from the example and put real secrets there:
```bash
cp .env.example .env
```

Or export variables manually:
```bash
export TG_BOT_TOKEN="YOUR_BOT_TOKEN"
export OPENWEATHER_API_KEY="YOUR_OPENWEATHERMAP_KEY"
./bot
```

Optional local-only developer commands (`testing1`, `testing2`) are disabled by default. To enable them for one Telegram chat:
```bash
export GEOBOT_DEV_CHAT_ID="YOUR_TELEGRAM_CHAT_ID"
```

### systemd Service

Create service file:
```bash
sudo nano /etc/systemd/system/geobot.service
```

```ini
[Unit]
Description=Geomagnetic Bot
After=network.target

[Service]
Type=simple
User=your_user
WorkingDirectory=/path/to/bot
Environment="TG_BOT_TOKEN=YOUR_TOKEN"
Environment="OPENWEATHER_API_KEY=YOUR_OPENWEATHERMAP_KEY"
# Optional:
# Environment="GEOBOT_DEV_CHAT_ID=YOUR_TELEGRAM_CHAT_ID"
ExecStart=/path/to/bot/bot
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

### Enable and Start

```bash
sudo systemctl daemon-reload
sudo systemctl enable geobot.service
sudo systemctl start geobot.service
```


### Telegram
Bot: @geomagnetic_belarus_bot
