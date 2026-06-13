# Security Policy

## Secrets

Never commit Telegram bot tokens, OpenWeatherMap keys, `.env` files, systemd
environment files, chat ids, or runtime state files.

Use `.env.example` only for placeholder names. Put real production secrets in
your server environment, for example in an untracked systemd environment file:

```ini
TG_BOT_TOKEN=replace_me
OPENWEATHER_API_KEY=replace_me
GEOBOT_DEV_CHAT_ID=
```

## Runtime Data

The bot creates local state files such as `users.txt`, `cities.txt`,
`notifications.txt`, `language.txt`, `live_messages.txt`, and rendered files
under `bot_screens/`. These files can contain Telegram chat identifiers or
message identifiers and must stay out of public commits.

## Reporting

If you find a leaked token or private identifier in a public fork, revoke the
token first, then remove the data from the repository history before publishing
again.
