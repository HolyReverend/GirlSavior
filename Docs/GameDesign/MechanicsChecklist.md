# Чек-лист реализации механик

Состояние проекта на 21 ноября 2026 года. Статусы сверены с кодом, игровыми ассетами и записями приёмки на карте Demo.

## Готово

Основное поведение реализовано и проверено в игре.

- [Перемещение персонажа](Mechanics/Player/CharacterMovement.md)
- [Прицеливание мышью](Mechanics/Player/CharacterAIM.md)
- [Следование камеры, кадрирование комнат и затухание переднего плана](Mechanics/Player/GameCamera.md)

## Требует доработки

В игре есть часть механики или её версия для тестов.

- [Стрельба персонажа](Mechanics/Weapons/CharacterShootSystem.md)
- [Обратная связь при стрельбе](Mechanics/Weapons/ShotFeedback.md)
- [Безопасные комнаты и малые костры](Mechanics/Expeditions/SafeRooms.md)
- [Предупреждения об атаках и реакция героя](Mechanics/Encounters/EnemyAttackFeedback.md)

## Не реализовано

В игровом проекте пока нет реализации.

### Герой, оружие и бой

- [Рывок персонажа](Mechanics/Player/CharacterCharge.md)
- [Выбор, вынос и открытие оружия](Mechanics/Weapons/Weapons.md)
- [Стартовый пистолет](Mechanics/Weapons/Pistol.md)
- [Дробовик](Mechanics/Weapons/Shotgun.md)
- [Автоматическая винтовка](Mechanics/Weapons/AssaultRifle.md)
- [Боевые комнаты](Mechanics/Encounters/CombatEncounters.md)
- [Типы обычных противников](Mechanics/Encounters/EnemyTypes.md)
- [Волны противников](Mechanics/Encounters/EnemyWaves.md)
- [Элитные враги](Mechanics/Encounters/EliteEnemies.md)
- [Облик и сигналы элитных врагов](Mechanics/Encounters/EliteEnemyPresentation.md)
- [Гибель врагов и героя](Mechanics/Encounters/DeathFeedback.md)
- [Эффекты усилений в бою](Mechanics/Encounters/UpgradeCombatFeedback.md)

### Вылазки

- [Типы экспедиций, выбор маршрута и открытие регионов](Mechanics/Expeditions/Expeditions.md)
- [Комнаты Бездны и их сборка](Mechanics/Expeditions/AbyssRoomDesign.md)
- [Шаблоны комнат и сборка маршрута экспедиции](Mechanics/Expeditions/ExpeditionLevelDesign.md)
- [Рост сложности в экспедиции](Mechanics/Expeditions/ExpeditionDifficulty.md)
- [Факел и его улучшение](Mechanics/Expeditions/Torch.md)
- [Спасение героинь](Mechanics/Expeditions/HeroineRescue.md)
- [Возвращение из экспедиции с добычей и потери при гибели](Mechanics/Expeditions/ExpeditionReturn.md)

### Лагерь и героини

- [Лагерь и защита Костра](Mechanics/Camp/CampHub.md)
- [Прогулка и взаимодействие в лагере](Mechanics/Camp/CampInteraction.md)
- [Стол экспедиций и подготовка вылазки](Mechanics/Camp/ExpeditionBoard.md)
- [Постройки и расширение лагеря](Mechanics/Camp/ShelterDevelopment.md)
- [Поддержка героинь и её улучшение](Mechanics/Heroines/HeroineSupport.md)
- [Сочетания героинь поддержки](Mechanics/Heroines/Heroines.md)
- [Способности и ранги Лиоры](Mechanics/Heroines/Liora.md)
- [Способности и ранги Иды](Mechanics/Heroines/Ida.md)
- [Способности и ранги Сайры](Mechanics/Heroines/Saira.md)
- [Личные просьбы героинь](Mechanics/Heroines/SupportRequests.md)
- [Отношения с героинями](Mechanics/Heroines/HeroineRelationships.md)
- [Интерфейс отношений в лагере](Mechanics/Camp/RelationshipInterface.md)
- [Режим визуальной новеллы](Mechanics/Camp/VisualNovel.md)
- [Спрайты персонажей в визуальной новелле](Mechanics/Camp/VisualNovelSprites.md)

### Прокачка, интерфейс и сохранения

- [Ресурсы для прокачки и их получение](Mechanics/Progression/UpgradeResources.md)
- [Уровень, пассивки героя и ранги оружия](Mechanics/Progression/HeroProgression.md)
- [Временные усиления в экспедиции](Mechanics/Progression/ExpeditionUpgrades.md)
- [Стартовые карточки временных усилений](Mechanics/Progression/ExpeditionUpgradeCards.md)
- [Интерфейс вылазки](Mechanics/Interface/ExpeditionHUD.md)
- [Сумка вылазки и лента добычи](Mechanics/Interface/ExpeditionInventory.md)
- [Показ опыта и повышения уровня в вылазке](Mechanics/Interface/ExpeditionExperience.md)
- [Окно выбора усиления](Mechanics/Interface/ExpeditionUpgradeChoice.md)
- [Показ действующих усилений в вылазке](Mechanics/Interface/ExpeditionActiveUpgrades.md)
- [Итоги вылазки](Mechanics/Interface/ExpeditionResults.md)
- [Главное меню, пауза и настройки](Mechanics/Interface/GameMenus.md)
- [Сохранение и продолжение в лагере, вылазке и визуальной новелле](Mechanics/Progression/SaveProgress.md)
