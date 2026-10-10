# Counter Zombie Mobile 32 players

<img align="right" width="160" height="160" src="assets/branding/logo-512.png" alt="Counter Zombie Mobile 32 players" />

Наша Android-версия на основе [CS16Client](https://github.com/Velaron/cs16-client).
Рабочая ветка: `feature/mobile-launcher`. Исходный `main` сохранён как отправная точка.

## Текущее состояние

- Собственное название, логотип и иконки Android.
- Отдельный идентификатор приложения: `ru.kurostreet.counterzombiemobile` (`.test` для тестовых APK).
- Одна иконка запуска; дополнительный ярлык Condition Zero отключён.
- Названия проекта, пакетов сборки и тексты поставляемых меню обновлены.
- Клиент пока запускает отдельный Xash3D FWGS. Встроенный движок, список наших серверов
  и автоматическая установка игровых ресурсов — следующие этапы.
- Число «32 players» является частью утверждённого названия. Вместимость сервера
  задаётся отдельно; нагрузочные испытания на 32 игрока ещё не проводились.

## Тестовые сборки

[Сборки Android](https://github.com/kurostreet/cs16-client/actions) ·
[Релизы](https://github.com/kurostreet/cs16-client/releases)

На этом этапе нужны [Xash3D FWGS](https://github.com/FWGS/xash3d-fwgs/releases/tag/continuous)
и игровые файлы из законно приобретённой копии игры. Материалы игрового кэша
не входят в этот репозиторий и не публикуются вместе с тестовым APK.

## Разработка

```sh
git clone --recurse-submodules https://github.com/kurostreet/cs16-client.git
cd cs16-client
git switch feature/mobile-launcher
cd android
./gradlew assembleGitDebug
```

План: [MOBILE_LAUNCHER_PLAN_RU.md](docs/MOBILE_LAUNCHER_PLAN_RU.md).
Оформление: [BRANDING_RU.md](docs/BRANDING_RU.md).

## Исходная основа и лицензии

Разработка основана на CS16Client; сведения об авторах, исходные лицензии и
авторские уведомления сохранены. [Исходное описание и список участников](docs/UPSTREAM_README.md),
[LICENSE](LICENSE), [исходный проект](https://github.com/Velaron/cs16-client).
Условия зависимостей и права на распространяемые игровые материалы проверяются
отдельно перед публичным выпуском.
