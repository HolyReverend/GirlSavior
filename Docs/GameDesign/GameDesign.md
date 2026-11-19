# Геймдизайн GirlSavior

Выберите раздел по задаче. Актуальные решения и игровые проверки отслеживаются в [реестре открытых вопросов](../OpenItems/Index.md).

## Основы

- [Концепт игры](ConceptDesign.md) — идея, мир и основной цикл.
- [Визуальное направление](VisualDirection.md) — графика, читаемость и оформление диалогов.

## Управление

- [Главное меню и пауза](Mechanics/Interface/GameMenus.md) — новая игра, продолжение, настройки и выход.
- [Интерфейс вылазки](Mechanics/Interface/ExpeditionHUD.md) — здоровье, поддержка, цель, добыча и указатели выхода.
- [Опыт и уровень в вылазке](Mechanics/Interface/ExpeditionExperience.md) — прибавки за врагов и цель, прогресс и сигнал повышения.
- [Окно выбора усиления](Mechanics/Interface/ExpeditionUpgradeChoice.md) — вид карточек, текущая сборка и предпросмотр.
- [Действующие усиления](Mechanics/Interface/ExpeditionActiveUpgrades.md) — значки в HUD, готовность золотой способности и окно по U.
- [Сумка и лента добычи](Mechanics/Interface/ExpeditionInventory.md) — автоматическое зачисление, последние поступления и просмотр инвентаря.
- [Итоги вылазки](Mechanics/Interface/ExpeditionResults.md) — результат цели, опыт, добыча и открытия после возвращения.
- [Игровая камера](Mechanics/Player/GameCamera.md) — ракурс, следование, границы кадра и перекрытия.
- [Перемещение персонажа](Mechanics/Player/CharacterMovement.md) — WASD, скорость, инерция и столкновения.
- [Рывок персонажа](Mechanics/Player/CharacterCharge.md) — запуск, движение, защита и перезарядка рывка.
- [Прицеливание персонажа](Mechanics/Player/CharacterAIM.md) — плоскость прицеливания, удержание направления и курсор.

## Оружие

- [Каталог оружия](Mechanics/Weapons/Weapons.md) — модели, боевые роли, находки и сравнение урона.
- [Стрельба персонажа](Mechanics/Weapons/CharacterShootSystem.md) — ввод, серия выстрелов и прерывания.
- [Обратная связь при стрельбе](Mechanics/Weapons/ShotFeedback.md) — отдача, вспышки, полёт снарядов и эффекты попадания.

## Героини

- [Каталог героинь](Mechanics/Heroines/Heroines.md) — карточки, миры, способности и сочетания.
- [Поддержка героинь](Mechanics/Heroines/HeroineSupport.md) — выбор состава, автоматические срабатывания и ранги.
- [Личные просьбы](Mechanics/Heroines/SupportRequests.md) — поиск предмета, первое улучшение и дополнительная романтическая сцена.
- [Отношения с героинями](Mechanics/Heroines/HeroineRelationships.md) — знакомство, доверие, дружба и романтические линии.

## Лагерь

- [Лагерь](Mechanics/Camp/CampHub.md) — подготовка, занятия между вылазками и роли героинь.
- [Постройки лагеря](Mechanics/Camp/ShelterDevelopment.md) — уровни зданий, условия, цены и источники ресурсов.
- [Прогулка и взаимодействие](Mechanics/Camp/CampInteraction.md) — 3D-площадка, подсказки и панели объектов.
- [Интерфейс отношений](Mechanics/Camp/RelationshipInterface.md) — поиск сцен и отслеживание историй.
- [Режим визуальной новеллы](Mechanics/Camp/VisualNovel.md) — чтение, история реплик, авто, пропуск, сохранение и выход.
- [Стол экспедиций и подготовка](Mechanics/Camp/ExpeditionBoard.md) — выбор миссии, оружие и карточки героинь.

## Экспедиции

- [Экспедиции](Mechanics/Expeditions/Expeditions.md) — запуск, четыре типа вылазок, условия целей и открытие регионов.
- [Факел](Mechanics/Expeditions/Torch.md) — свет в Бездне, первая находка и дальность света.
- [Безопасные комнаты](Mechanics/Expeditions/SafeRooms.md) — розжиг малого костра и временная защита.
- [Рост сложности](Mechanics/Expeditions/ExpeditionDifficulty.md) — знакомство с врагами и профили давления.
- [Левел-дизайн экспедиций](Mechanics/Expeditions/ExpeditionLevelDesign.md) — планировки, подходы и сборка маршрута.
- [Спасение героинь](Mechanics/Expeditions/HeroineRescue.md) — зарядка портала, эвакуация и исход испытания.
- [Возвращение из экспедиции](Mechanics/Expeditions/ExpeditionReturn.md) — выходы, перенос добычи и последствия гибели.

## Боевые встречи

- [Боевые комнаты](Mechanics/Encounters/CombatEncounters.md) — пространство боя и условия продвижения.
- [Типы противников](Mechanics/Encounters/EnemyTypes.md) — боевые роли, атаки и поведение.
- [Элитные враги](Mechanics/Encounters/EliteEnemies.md) — Камнелом и Ткач разломов, их способности и условия встречи.
- [Облик и сигналы элит](Mechanics/Encounters/EliteEnemyPresentation.md) — силуэты, анимации, предупреждения и звук Камнелома и Ткача.
- [Атаки врагов и реакция героя](Mechanics/Encounters/EnemyAttackFeedback.md) — предупреждения в темноте, удары, снаряды и попадания по барьеру и здоровью.
- [Эффекты усилений в бою](Mechanics/Encounters/UpgradeCombatFeedback.md) — срабатывание пассивных карт, вид и звук «Рунного импульса» и «Ответного выстрела».
- [Гибель врагов и героя](Mechanics/Encounters/DeathFeedback.md) — подтверждение добивания, тела и причина гибели героя.
- [Волны противников](Mechanics/Encounters/EnemyWaves.md) — темп подкреплений, лимиты и появление за кадром.

## Прогрессия

- [Система сохранений](Mechanics/Progression/SaveProgress.md) — запись прогресса лагеря, вылазки и ВН, выход и продолжение.

- [Прокачка героя](Mechanics/Progression/HeroProgression.md) — уровень, пассивки, ранги оружия и предел собственной силы.
- [Ресурсы для прокачки](Mechanics/Progression/UpgradeResources.md) — источники материалов и стоимость рангов.
- [Усиления в экспедиции](Mechanics/Progression/ExpeditionUpgrades.md) — обязательный выбор на паузе, предпросмотр характеристик и действующие карты.
- [Стартовые карточки усилений](Mechanics/Progression/ExpeditionUpgradeCards.md) — десять карт, значения эффектов и сочетания.

Числовые ориентиры подлежат игровой проверке. Границы первого прототипа определяет [технический план](../Technical/Prototype%231/ImplementationPlan.md).
