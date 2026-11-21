# GS-012 · Автоматическое исчезновение снаряда

**Тема** — Unreal / стрельба

**Статус** — Закрыто

## Результат

На этапе 4 проверили настройку времени жизни тестового снаряда в 1,5 с. На CP1 команда `GirlSavior.TestDemoLifetime` выпустила сохранённый `BP_TestProjectile` в свободное пространство Demo. Через 1,25 игровой секунды снаряд прошёл 2504,87 см. К проверке через 1,65 с он исчез по таймеру.

Запись проверки находится в `GirlSavior/Saved/CP1/DemoProjectileLifetime.txt`. [План и результаты боевого прототипа](../Technical/CombatPrototype/ImplementationPlan.md)

[Приёмка стрельбы](../Technical/Prototype%231/Stage04-CharacterShooting.md#acceptance-and-continuation)
