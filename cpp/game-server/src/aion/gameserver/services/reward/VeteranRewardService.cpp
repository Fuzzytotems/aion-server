#include "aion/gameserver/services/reward/VeteranRewardService.h"

#include <chrono>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/dao/VeteranRewardDAO.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/mail/SystemMailService.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::services::reward {

using model::templates::rewards::RewardItem;
using LocalDateTime = std::chrono::local_time<std::chrono::milliseconds>;

runtime::ArrayList<runtime::Ref<runtime::RcArrayList<runtime::Ref<RewardItem>>>> VeteranRewardService::rewards{
	AION_LOCK_CLASS(VeteranRewardService::rewards)};
runtime::ArrayList<runtime::Ref<RewardItem>> VeteranRewardService::randomRewards{AION_LOCK_CLASS(VeteranRewardService::randomRewards)};

namespace {

/**
 * Java ChronoUnit.MONTHS.between(start, end) for two times of the server zone: LocalDateTime.until (the end date moves one day towards the start
 * when its time of day has not reached the start's) and LocalDate.monthsUntil ((packed2 - packed1) / 32 with packed = prolepticMonth * 32 + day).
 */
int64_t monthsBetween(LocalDateTime start, LocalDateTime end) {
	std::chrono::local_days startDay = std::chrono::floor<std::chrono::days>(start);
	std::chrono::local_days endDay = std::chrono::floor<std::chrono::days>(end);
	std::chrono::milliseconds startTime = start - startDay;
	std::chrono::milliseconds endTime = end - endDay;
	if (endDay > startDay && endTime < startTime)
		endDay -= std::chrono::days(1);
	else if (endDay < startDay && endTime > startTime)
		endDay += std::chrono::days(1);
	auto packed = [](std::chrono::local_days day) {
		std::chrono::year_month_day ymd(day);
		int64_t prolepticMonth = static_cast<int64_t>(static_cast<int32_t>(ymd.year())) * 12 + static_cast<int64_t>(static_cast<unsigned>(ymd.month())) - 1;
		return prolepticMonth * 32 + static_cast<int64_t>(static_cast<unsigned>(ymd.day()));
	};
	return (packed(endDay) - packed(startDay)) / 32;
}

} // namespace

/** Prevent instantiation. C++: also runs the Java static initializer block (VeteranRewardService.java:27), since getInstance() is the only entry */
VeteranRewardService::VeteranRewardService() {
	for (int32_t i = 0; i < 60; i++)
		rewards.add(runtime::RcArrayList<runtime::Ref<RewardItem>>::create());
	auto month = [](int32_t index, int32_t itemId, int64_t count) { rewards.get(index)->add(RewardItem::create(itemId, count)); };

	// month 1
	month(0, 169630007, 1); // [Expand Card] Expand Cube Ticket (lvl 4)
	month(0, 169620094, 1); // Crafting Boost Charm III - 100%
	month(0, 161001001, 5); // Revival Stone
	month(0, 162002030, 50); // [Event] Premium Restoration Serum

	// month 2
	month(1, 190020075, 1); // Flash Bogel Egg
	month(1, 169600064, 1); // [Emotion Card] Playing Dead
	month(1, 162000137, 25); // Sublime Life Serum
	month(1, 162000139, 25); // Sublime Mana Serum

	// month 3
	month(2, 125040038, 1); // Devil Horns
	month(2, 186000199, 100); // Legion Coin
	month(2, 166020003, 5); // [Event] Omega Enchantment Stone
	month(2, 169620072, 3); // AP Boost Charm II - 30%

	// month 4
	month(3, 169640006, 1); // [Expand Card] Expand Warehouse Ticket (lvl 4)
	month(3, 186000242, 15); // Ceramium Medal
	month(3, 188052719, 5); // [Event] Dye Bundle
	month(3, 162002018, 50); // [Event] Wormwood Dish

	// month 5
	month(4, 166030007, 5); // [Event] Tempering Solution
	month(4, 169600103, 1); // [Emotion Card] Diving
	month(4, 161001001, 5); // Revival Stone
	month(4, 188053526, 5); // [Event] Aion's Steel Form Candy Box
	month(4, 186000051, 5); // Major Ancient Crown

	// month 6
	month(5, 187000057, 1); // Kahrun's Wing
	month(5, 166020003, 5); // [Event] Omega Enchantment Stone
	month(5, 164002264, 25); // Flame Pillar Firecracker
	month(5, 169670000, 1); // Name Change Ticket

	// month 7
	month(6, 169630007, 1); // [Expand Card] Expand Cube Ticket (lvl 4)
	month(6, 169600065, 1); // [Emotion Card] Sing
	month(6, 169620072, 3); // AP Boost Charm II - 30%
	month(6, 162000137, 25); // Sublime Life Serum
	month(6, 162000139, 25); // Sublime Mana Serum

	// month 8
	month(7, 190000048, 1); // Golden Nyanco Egg
	month(7, 186000242, 15); // Ceramium Medal
	month(7, 188052719, 5); // [Event] Dye Bundle
	month(7, 186000199, 100); // Legion Coin

	// month 9
	month(8, 166020003, 5); // [Event] Omega Enchantment Stone
	month(8, 169600087, 1); // [Emotion Card] 'Bad Girl' Dance
	month(8, 188052761, 5); // [Event] Bonus Entry Scroll Bundle
	month(8, 162002018, 50); // [Event] Wormwood Dish

	// month 10
	month(9, 169640006, 1); // [Expand Card] Expand Warehouse Ticket (lvl 4)
	month(9, 169650007, 1); // [Event] Plastic Surgery Ticket
	month(9, 186000051, 5); // Major Ancient Crown
	month(9, 161001001, 5); // Revival Stone

	// month 11
	month(10, 166030007, 5); // [Event] Tempering Solution
	month(10, 164002284, 25); // [Event] Ornate Firecrackers
	month(10, 188053526, 5); // [Event] Aion's Steel Form Candy Box
	month(10, 169620072, 3); // AP Boost Charm II - 30%

	// month 12
	month(11, 190100107, 1); // Emerald Crestlich
	month(11, 169600062, 1); // [Emotion Card] Play Harp
	month(11, 169610343, 1); // [Title] Forgotten Hero
	month(11, 166020003, 5); // [Event] Omega Enchantment Stone

	// month 13
	month(12, 186000242, 15); // Ceramium Medal
	month(12, 169660003, 1); // [Event] Gender Switch Ticket
	month(12, 162000137, 25); // Sublime Life Serum
	month(12, 162000139, 25); // Sublime Mana Serum

	// month 14
	month(13, 166030007, 5); // [Event] Tempering Solution
	month(13, 169600063, 1); // [Emotion Card] Play the Saxophone
	month(13, 188052761, 5); // [Event] Bonus Entry Scroll Bundle
	month(13, 162002018, 50); // [Event] Wormwood Dish

	// month 15
	month(14, 110900876, 1); // Nyerkcarrier
	month(14, 190020156, 1); // [Event] Medalist Shugo Egg
	month(14, 166020003, 5); // [Event] Omega Enchantment Stone
	month(14, 161001001, 5); // Revival Stone

	// month 16
	month(15, 188053526, 5); // [Event] Aion's Steel Form Candy Box
	month(15, 169600060, 1); // [Emotion Card] Play the Drum
	month(15, 188052719, 5); // [Event] Dye Bundle
	month(15, 186000051, 5); // Major Ancient Crown
	month(15, 169620072, 3); // AP Boost Charm II - 30%

	// month 17
	month(16, 166030007, 5); // [Event] Tempering Solution
	month(16, 164002284, 25); // [Event] Ornate Firecrackers
	month(16, 186000242, 15); // Ceramium Medal
	month(16, 162000137, 25); // Sublime Life Serum
	month(16, 162000139, 25); // Sublime Mana Serum

	// month 18
	month(17, 187060162, 1); // Wings of Agony
	month(17, 168310018, 1); // Major Blessed Augment: Level 2
	month(17, 166020003, 5); // [Event] Omega Enchantment Stone
	month(17, 162002018, 50); // [Event] Wormwood Dish

	// month 19
	month(18, 125050026, 1); // Elcoro Hat
	month(18, 186000077, 1); // Hot Heart of Magic
	month(18, 186000247, 5); // Major Danuar Relic
	month(18, 161001001, 5); // Revival Stone

	// month 20
	month(19, 186000242, 15); // Ceramium Medal
	month(19, 166030007, 5); // [Event] Tempering Solution
	month(19, 188052719, 5); // [Event] Dye Bundle
	month(19, 162000137, 25); // Sublime Life Serum
	month(19, 162000139, 25); // Sublime Mana Serum

	// month 21
	month(20, 169630007, 1); // [Expand Card] Expand Cube Ticket (lvl 4)
	month(20, 169600039, 1); // [Emotion Card] Chew Bubblegum
	month(20, 186000238, 150); // Conqueror's Herb
	month(20, 162002018, 50); // [Event] Wormwood Dish

	// month 22
	month(21, 169640006, 1); // [Expand Card] Expand Warehouse Ticket (lvl 4)
	month(21, 152012593, 3); // Valor's Heart
	month(21, 152012587, 3); // Wind Eternity
	month(21, 166020003, 5); // [Event] Omega Enchantment Stone
	month(21, 164002284, 25); // [Event] Ornate Firecrackers

	// month 23
	month(22, 188508017, 1); // [Motion Card] Stormbringer
	month(22, 188053609, 3); // [Event] Level 60 Composite Manastone Bundle
	month(22, 166200009, 3); // Mythic Weapon Tuning Scroll
	month(22, 166200010, 3); // Mythic Armor Tuning Scroll

	// month 24
	month(23, 169650007, 1); // [Event] Plastic Surgery Ticket
	month(23, 186000242, 15); // Ceramium Medal
	month(23, 152012586, 2); // Wind Breath
	month(23, 152012581, 2); // Fire Breath
	month(23, 186000238, 150); // Conqueror's Mark

	// month 25
	month(24, 169630007, 1); // [Expand Card] Expand Cube Ticket (lvl 4)
	month(24, 166030007, 5); // [Event] Tempering Solution
	month(24, 169620072, 3); // AP Boost Charm II - 30%
	month(24, 162002018, 50); // [Event] Wormwood Dish

	// month 26
	month(25, 169640006, 1); // [Expand Card] Expand Warehouse Ticket (lvl 4)
	month(25, 152012593, 3); // Valor's Heart
	month(25, 152012590, 3); // Wind Origin
	month(25, 161001001, 5); // Revival Stone
	month(25, 166020003, 5); // [Event] Omega Enchantment Stone

	// month 27
	month(26, 188500014, 1); // [Motion Card] The Dragon's Set
	month(26, 186000247, 5); // Major Danuar Relic
	month(26, 164002116, 25); // [Event] Rx: Accelerox
	month(26, 164002117, 25); // [Event] Rx: Blitzopan
	month(26, 164002118, 25); // [Event] Rx: Castafodin

	// month 28
	month(27, 110900731, 1); // Cogwheel Couture
	month(27, 166020003, 5); // [Event] Omega Enchantment Stone
	month(27, 152012586, 2); // Wind Breath
	month(27, 152012581, 2); // Fire Breath

	// month 29
	month(28, 169600186, 1); // [Emotion Card] Sing "Good Day"
	month(28, 166200009, 3); // Mythic Weapon Tuning Scroll
	month(28, 166200010, 3); // Mythic Armor Tuning Scroll
	month(28, 162002018, 50); // [Event] Wormwood Dish

	// month 30
	month(29, 169610137, 1); // [Title Card] Aion's Chosen
	month(29, 188053526, 5); // [Event] Aion's Steel Form Candy Box
	month(29, 162000137, 25); // Sublime Life Serum
	month(29, 162000139, 25); // Sublime Mana Serum

	// month 31
	month(30, 166030007, 3); // [Event] Tempering Solution
	month(30, 166020003, 5); // [Event] Omega Enchantment Stone
	month(30, 164002116, 25); // [Event] Rx: Accelerox
	month(30, 164002117, 25); // [Event] Rx: Blitzopan
	month(30, 164002118, 25); // [Event] Rx: Castafodin

	// month 32
	month(31, 169600086, 1); // [Emotion Card] 'Shut Up' Dance
	month(31, 186000242, 15); // Ceramium Medal
	month(31, 188053609, 3); // [Event] Level 60 Composite Manastone Bundle
	month(31, 186000247, 5); // Major Danuar Relic
	month(31, 188052761, 5); // [Event] Bonus Entry Scroll Bundle

	// month 33
	month(32, 187060178, 1); // Aether Glider
	month(32, 166030007, 5); // [Event] Tempering Solution
	month(32, 162002018, 50); // [Event] Wormwood Dish
	month(32, 161001001, 5); // Revival Stone

	// month 34
	month(33, 168310018, 1); // Major Blessed Augment: Level 2
	month(33, 188052638, 1); // [Event] Fabled Godstone Bundle
	month(33, 188052719, 5); // [Event] Dye Bundle
	month(33, 164002284, 25); // [Event] Ornate Firecrackers

	// month 35
	month(34, 169600102, 1); // [Emotion Card] Floor Sweep
	month(34, 188053526, 5); // [Event] Aion's Steel Form Candy Box
	month(34, 164002272, 25); // [Event] Enduring Greater Raging Wind Scroll
	month(34, 162000141, 25); // Sublime Wind Serum
	month(34, 186000238, 150); // Conqueror's Mark

	// month 36
	month(35, 190100042, 1); // Legion Pagati
	month(35, 166030007, 3); // [Event] Tempering Solution
	month(35, 166020003, 5); // [Event] Omega Enchantment Stone
	month(35, 169620072, 3); // AP Boost Charm II - 30%

	// month 37
	month(36, 169650007, 1); // [Event] Plastic Surgery Ticket
	month(36, 186000247, 5); // Major Danuar Relic
	month(36, 164002116, 25); // [Event] Rx: Accelerox
	month(36, 164002117, 25); // [Event] Rx: Blitzopan
	month(36, 164002118, 25); // [Event] Rx: Castafodin

	// month 38
	month(37, 165020016, 1); // Accessory Wrapping Scroll (Eternal/Lv. 65 and lower)
	month(37, 188053610, 3); // [Event] Level 70 Composite Manastone Bundle
	month(37, 188053526, 5); // [Event] Aion's Steel Form Candy Box
	month(37, 186000399, 100); // Honorable Conqueror's Mark

	// month 39
	month(38, 110900695, 1); // Biker Costume
	month(38, 166030007, 5); // [Event] Tempering Solution
	month(38, 169620072, 3); // AP Boost Charm II - 30%
	month(38, 162002018, 50); // [Event] Wormwood Dish

	// month 40
	month(39, 125045415, 1); // Biker Hat
	month(39, 186000242, 15); // Ceramium Medal
	month(39, 188052719, 5); // [Event] Dye Bundle
	month(39, 186000199, 150); // Legion Coin

	// month 41
	month(40, 165020015, 1); // Armor Wrapping Scroll (Eternal/Lv. 65 and lower)
	month(40, 152012593, 3); // Valor's Heart
	month(40, 152012587, 3); // Wind Eternity
	month(40, 166020003, 5); // [Event] Omega Enchantment Stone

	// month 42
	month(41, 168310018, 1); // Major Blessed Augment: Level 2
	month(41, 186000051, 5); // Major Ancient Crown
	month(41, 166200009, 3); // Mythic Weapon Tuning Scroll
	month(41, 166200010, 3); // Mythic Armor Tuning Scroll

	// month 43
	month(42, 188508005, 1); // [Motion Card] Socialite
	month(42, 188053526, 5); // [Event] Aion's Steel Form Candy Box
	month(42, 169620072, 3); // AP Boost Charm II - 30%
	month(42, 152012590, 3); // Wind Origin
	month(42, 186000238, 150); // Conqueror's Mark

	// month 44
	month(43, 165020014, 1); // Weapon Wrapping Scroll (Eternal/Lv. 65 and lower)
	month(43, 186000242, 15); // Ceramium Medal
	month(43, 188053610, 3); // [Event] Level 70 Composite Manastone Bundle
	month(43, 186000247, 5); // Major Danuar Relic
	month(43, 188052761, 5); // [Event] Bonus Entry Scroll Bundle

	// month 45
	month(44, 169600217, 1); // [Emotion Card] Summer Vacation
	month(44, 161001001, 5); // Revival Stone
	month(44, 164002272, 25); // [Event] Enduring Greater Raging Wind Scroll
	month(44, 162000141, 25); // Sublime Wind Serum

	// month 46
	month(45, 170100041, 1); // Club Speaker Cabinet
	month(45, 152012586, 2); // Wind Breath
	month(45, 152012581, 2); // Fire Breath
	month(45, 166020003, 5); // [Event] Omega Enchantment Stone

	// month 47
	month(46, 186000242, 15); // Ceramium Medal
	month(46, 166030007, 5); // [Event] Tempering Solution
	month(46, 186000242, 5); // Ceramium Medal
	month(46, 162000137, 25); // Sublime Life Serum
	month(46, 162000139, 25); // Sublime Mana Serum

	// month 48
	month(47, 169610158, 1); // [Title Card] Prestigious Adept
	month(47, 162002018, 50); // [Event] Wormwood Dish
	month(47, 166020003, 5); // [Event] Omega Enchantment Stone
	month(47, 186000399, 100); // Honorable Conqueror's Mark

	// month 49
	month(48, 169600098, 1); // [Emotion Card] Hug Me
	month(48, 161001001, 5); // Revival Stone
	month(48, 166030007, 5); // [Event] Tempering Solution
	month(48, 186000247, 5); // Major Danuar Relic
	month(48, 169620072, 3); // AP Boost Charm II - 30%

	// month 50
	month(49, 165020015, 1); // Armor Wrapping Scroll (Eternal/Lv. 65 and lower)
	month(49, 188053526, 5); // [Event] Aion's Steel Form Candy Box
	month(49, 162000137, 25); // Sublime Life Serum
	month(49, 162000139, 25); // Sublime Mana Serum

	// month 51
	month(50, 110900603, 1); // Lawful Uniform
	month(50, 166020003, 5); // [Event] Omega Enchantment Stone
	month(50, 164002116, 25); // [Event] Rx: Accelerox
	month(50, 164002117, 25); // [Event] Rx: Blitzopan
	month(50, 164002118, 25); // [Event] Rx: Castafodin

	// month 52
	month(51, 125045283, 1); // Lawful Headgear
	month(51, 186000242, 15); // Ceramium Medal
	month(51, 188052719, 5); // [Event] Dye Bundle
	month(51, 164002284, 15); // [Event] Ornate Firecrackers
	month(51, 186000409, 150); // Daeva's Respite Coin

	// month 53
	month(52, 165020016, 1); // Accessory Wrapping Scroll (Eternal/Lv. 65 and lower)
	month(52, 161001001, 5); // Revival Stone
	month(52, 186000051, 5); // Major Ancient Crown
	month(52, 188052719, 5); // [Event] Dye Bundle

	// month 54
	month(53, 169670000, 1); // Name Change Ticket
	month(53, 162002018, 50); // [Event] Wormwood Dish
	month(53, 166020003, 5); // [Event] Omega Enchantment Stone
	month(53, 164002272, 25); // [Event] Enduring Greater Raging Wind Scroll
	month(53, 162000141, 25); // Sublime Wind Serum

	// month 55
	month(54, 168310018, 1); // Major Blessed Augment: Level 2
	month(54, 162000137, 25); // Sublime Life Serum
	month(54, 162000139, 25); // Sublime Mana Serum
	month(54, 188053610, 3); // [Event] Level 70 Composite Manastone Bundle
	month(54, 164002284, 15); // [Event] Ornate Firecrackers

	// month 56
	month(55, 165020014, 1); // Weapon Wrapping Scroll (Eternal/Lv. 65 and lower)
	month(55, 186000242, 15); // Ceramium Medal
	month(55, 169620072, 3); // AP Boost Charm II - 30%
	month(55, 188053618, 1); // Honorable Elim's Idian Bundle

	// month 57
	month(56, 166200009, 3); // Mythic Weapon Tuning Scroll
	month(56, 161001001, 5); // Revival Stone
	month(56, 166020003, 5); // [Event] Omega Enchantment Stone
	month(56, 166100023, 1000); // [Stamp] High Grade Enchanting Supplement (Mythic)

	// month 58
	month(57, 190010001, 1); // Potbelly Inquin Egg
	month(57, 188052761, 5); // [Event] Bonus Entry Scroll Bundle
	month(57, 188053526, 5); // [Event] Aion's Steel Form Candy Box
	month(57, 186000247, 5); // Major Danuar Relic
	month(57, 166150026, 2); // [Stamp] Greater Felicitous Socketing (Heroic)

	// month 59
	month(58, 166200010, 3); // Mythic Armor Tuning Scroll
	month(58, 166020003, 5); // [Event] Omega Enchantment Stone
	month(58, 188052719, 5); // [Event] Dye Bundle
	month(58, 166030007, 5); // [Event] Tempering Solution

	// month 60
	month(59, 188053996, 1); // Emperor Trillirunerk's Feather Box
	month(59, 162002018, 50); // [Event] Wormwood Dish
	month(59, 186000399, 125); // Honorable Conqueror's Mark
	month(59, 166150027, 2); // [Stamp] Greater Felicitous Socketing (Mythic)

	// random rewards for month 61+
	randomRewards.add(RewardItem::create(161001001, 5)); // Revival Stone
	randomRewards.add(RewardItem::create(162000137, 15)); // Sublime Life Serum
	randomRewards.add(RewardItem::create(162000139, 15)); // Sublime Mana Serum
	randomRewards.add(RewardItem::create(162000141, 15)); // Sublime Wind Serum
	randomRewards.add(RewardItem::create(164002167, 15)); // Drana Coffee
	randomRewards.add(RewardItem::create(188054198, 3)); // Greater Scroll Bundle
	randomRewards.add(RewardItem::create(186000051, 5)); // Major Ancient Crown
	randomRewards.add(RewardItem::create(186000247, 5)); // Major Danuar Relic
	randomRewards.add(RewardItem::create(188053666, 2)); // [Event] Ceramium Medal Box
	randomRewards.add(RewardItem::create(188053667, 1)); // [Event] Mithril Medal Box
	randomRewards.add(RewardItem::create(186000243, 10)); // Fragmented Ceramium
	randomRewards.add(RewardItem::create(186000236, 75)); // Blood Mark
	randomRewards.add(RewardItem::create(188053610, 3)); // [Event] Level 70 Composite Manastone Bundle
	randomRewards.add(RewardItem::create(169620094, 1)); // Crafting Boost Charm III - 100%
	randomRewards.add(RewardItem::create(169620082, 1)); // Gathering Boost Charm II - 100%
	randomRewards.add(RewardItem::create(169620072, 1)); // AP Boost Charm II - 30%
	randomRewards.add(RewardItem::create(166020003, 5)); // [Event] Omega Enchantment Stone
	randomRewards.add(RewardItem::create(166030007, 5)); // [Event] Tempering Solution
	randomRewards.add(RewardItem::create(166500005, 5)); // [Event] Amplification Stone
	randomRewards.add(RewardItem::create(188053526, 5)); // [Event] Aion's Steel Form Candy Box
	randomRewards.add(RewardItem::create(188052719, 5)); // [Event] Dye Bundle
	randomRewards.add(RewardItem::create(186000238, 150)); // Conqueror's Herb
	randomRewards.add(RewardItem::create(186000399, 125)); // Honorable Conqueror's Mark
	randomRewards.add(RewardItem::create(186000409, 50)); // Daeva's Respite Coin
	randomRewards.add(RewardItem::create(188052761, 3)); // [Event] Bonus Entry Scroll Bundle
	randomRewards.add(RewardItem::create(166150018, 3)); // Assured Greater Felicitous Socketing (Eternal)
	randomRewards.add(RewardItem::create(166150019, 3)); // Assured Greater Felicitous Socketing (Mythic)
	randomRewards.add(RewardItem::create(166100020, 250)); // [Stamp] High Grade Enchanting Supplement (Eternal)
	randomRewards.add(RewardItem::create(166100023, 250)); // [Stamp] High Grade Enchanting Supplement (Mythic)
}

VeteranRewardService& VeteranRewardService::getInstance() {
	static VeteranRewardService instance; // Java SingletonHolder
	return instance;
}

void VeteranRewardService::tryReward(model::gameobjects::player::Player& player) {
	if (player.getLevel() != 65)
		return;

	LocalDateTime now = utils::time::ServerTime::now().get_local_time();
	std::optional<commons::database::Timestamp> creationDate = player.getCreationDate();
	if (!creationDate) // Java: NullPointerException in ServerTime.atDate
		throw runtime::NullPointerException("Player " + player.getName() + " has no creation date");
	LocalDateTime charCreationTime = utils::time::ServerTime::atDate(*creationDate).get_local_time();
	if (monthsBetween(charCreationTime, now) < 1) // return if char is younger than a month
		return;

	LocalDateTime accCreationTime = utils::time::ServerTime::ofEpochMilli(player.getAccount()->getCreationDate()).get_local_time();
	int32_t maxMonthsToReceive = static_cast<int32_t>(monthsBetween(accCreationTime, now));
	if (maxMonthsToReceive < 1) // return if account is younger than a month
		return;

	int32_t receivedMonths = dao::VeteranRewardDAO::loadReceivedMonths(player); // -1 means error
	if (receivedMonths < 0 || receivedMonths >= maxMonthsToReceive)
		return;

	if (dao::VeteranRewardDAO::storeReceivedMonths(player, maxMonthsToReceive))
		for (int32_t i = receivedMonths; i < maxMonthsToReceive; i++) {
			std::vector<runtime::Ptr<RewardItem>> items;
			if (i < 60) {
				items = rewards.get(i)->snapshot();
			} else {
				items = randomRewards.snapshot();
				while (items.size() > static_cast<size_t>(RANDOM_ITEMS_PER_MONTH))
					items.erase(items.begin() + commons::utils::Rnd::nextInt(static_cast<int32_t>(items.size())));
			}
			if (player.getMailbox()->getLetters().size() >= 100) { // abort on mailbox overflow and save the correct month
				dao::VeteranRewardDAO::storeReceivedMonths(player, i);
				return;
			}
			for (const runtime::Ptr<RewardItem>& item : items)
				mail::SystemMailService::sendMail("Beyond Aion", player.getName(), "Veteran Reward",
					"Greetings Daeva!\n\nIt has been over " + (i == 0 ? std::string("a month") : std::to_string(i + 1) + " months") +
						" now, since you joined us.\nWe send you this and hope you stay with us even longer :)\n\n~ Beyond Aion",
					item->getId(), item->getCount(), 0, model::gameobjects::LetterType::BLACKCLOUD);
		}
}

} // namespace aion::gameserver::services::reward
