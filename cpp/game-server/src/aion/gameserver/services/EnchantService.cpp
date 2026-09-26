#include "aion/gameserver/services/EnchantService.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/EnchantData.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/geoEngine/math/JavaFloat.h"
#include "aion/gameserver/model/enchants/EnchantEffect.h"
#include "aion/gameserver/model/enchants/EnchantStat.h"
#include "aion/gameserver/model/enchants/EnchantmentStoneInfo.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/listeners/ItemEquipmentListener.h"
#include "aion/gameserver/model/templates/item/ExceedEnchantSkillSetType.h"
#include "aion/gameserver/model/templates/item/ItemQuality.h"
#include "aion/gameserver/model/templates/item/ItemQualityInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/EnchantItemAction.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/SkillLearnService.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/services/item/ItemSocketService.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::services {

namespace {

namespace Rnd = commons::utils::Rnd;
using configs::main::RatesConfig;
using geoEngine::math::JavaFloat;
using model::enchants::EnchantmentStone;
using model::gameobjects::Item;
using model::gameobjects::Persistable_PersistentState;
using model::gameobjects::player::Player;
using model::templates::item::ExceedEnchantSkillSetType;
using model::templates::item::ItemQuality;
using model::templates::item::ItemTemplate;
using model::templates::item::actions::EnchantItemAction;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using utils::PacketSendUtility;
using utils::audit::AuditLogger;

/** Java int addition (wraps) */
int32_t javaAdd(int32_t a, int32_t b) {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

/** Java int subtraction (wraps) */
int32_t javaSub(int32_t a, int32_t b) {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

/** Java int multiplication (wraps) */
int32_t javaMul(int32_t a, int32_t b) {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/** Java Rates.get(player, RatesConfig.X) on a snapshot of the reloadable config value */
float rate(Player& player, const commons::configuration::ConfigValue<std::vector<float>>& membershipRates) {
	std::shared_ptr<const std::vector<float>> rates = membershipRates.get();
	return model::gameobjects::player::get(player, rates ? *rates : std::vector<float>());
}

/** Java `itemTemplate.getItemQuality()` handed to a switch or dereferenced: a template without a quality is a NullPointerException */
ItemQuality qualityOf(const ItemTemplate& itemTemplate) {
	std::optional<ItemQuality> quality = itemTemplate.getItemQuality();
	if (!quality)
		throw runtime::NullPointerException("itemTemplate.getItemQuality()");
	return *quality;
}

/** Java `template.getActions().getEnchantAction()`: a template without actions is a NullPointerException */
const EnchantItemAction* enchantActionOf(const ItemTemplate& itemTemplate) {
	const model::templates::item::actions::ItemActions* actions = itemTemplate.getActions();
	if (actions == nullptr)
		throw runtime::NullPointerException("itemTemplate.getActions()");
	return actions->getEnchantAction();
}

/** Java `targetItem.getFusionedItemTemplate()` dereferenced right away: an item without a fused weapon is a NullPointerException */
const ItemTemplate& fusionedTemplateOf(const Item& item) {
	const ItemTemplate* fusioned = item.getFusionedItemTemplate();
	if (fusioned == nullptr)
		throw runtime::NullPointerException("item.getFusionedItemTemplate()");
	return *fusioned;
}

/** Java (result ? "Success" : "Fail") + " (success chance:" + successChance + "%)" for AdminConfig.ENCHANT_INFO */
std::string enchantInfo(bool result, float successChance) {
	return std::string(result ? "Success" : "Fail") + " (success chance:" + JavaFloat::toString(successChance) + "%)";
}

/** The skill arrays of getEquipBuff's switch (EnchantService.java:427-543), per exceed enchant skill set */
std::array<int32_t, 3> exceedEnchantSkills(ExceedEnchantSkillSetType type) {
	switch (type) {
		case ExceedEnchantSkillSetType::RANK1_SET1_MAGICAL_GLOVES:
			return {13042, 13046, 13055};
		case ExceedEnchantSkillSetType::RANK1_SET1_MAGICAL_PANTS:
			return {13071, 13075, 13078};
		case ExceedEnchantSkillSetType::RANK1_SET1_MAGICAL_SHOES:
			return {13108, 13118, 13121};
		case ExceedEnchantSkillSetType::RANK1_SET1_MAGICAL_SHOULDER:
			return {13104, 13097, 13098};
		case ExceedEnchantSkillSetType::RANK1_SET1_MAGICAL_TORSO:
			return {13128, 13132, 13144};
		case ExceedEnchantSkillSetType::RANK1_SET1_MAGICAL_WEAPON:
			return {13011, 13012, 13027};
		case ExceedEnchantSkillSetType::RANK1_SET1_PHYSICAL_GLOVES:
			return {13042, 13046, 13055};
		case ExceedEnchantSkillSetType::RANK1_SET1_PHYSICAL_PANTS:
			return {13071, 13075, 13078};
		case ExceedEnchantSkillSetType::RANK1_SET1_PHYSICAL_SHOES:
			return {13108, 13118, 13121};
		case ExceedEnchantSkillSetType::RANK1_SET1_PHYSICAL_SHOULDER:
			return {13104, 13097, 13098};
		case ExceedEnchantSkillSetType::RANK1_SET1_PHYSICAL_TORSO:
			return {13128, 13132, 13144};
		case ExceedEnchantSkillSetType::RANK1_SET1_PHYSICAL_WEAPON:
			return {13011, 13012, 13027};
		case ExceedEnchantSkillSetType::RANK1_SET2_MAGICAL_GLOVES:
			return {13046, 13058, 13056};
		case ExceedEnchantSkillSetType::RANK1_SET2_MAGICAL_PANTS:
			return {13075, 13061, 13067};
		case ExceedEnchantSkillSetType::RANK1_SET2_MAGICAL_SHOES:
			return {13121, 13114, 13119};
		case ExceedEnchantSkillSetType::RANK1_SET2_MAGICAL_SHOULDER:
			return {13104, 13094, 13102};
		case ExceedEnchantSkillSetType::RANK1_SET2_MAGICAL_TORSO:
			return {13144, 13135, 13133};
		case ExceedEnchantSkillSetType::RANK1_SET2_MAGICAL_WEAPON:
			return {13029, 13003, 13023};
		case ExceedEnchantSkillSetType::RANK1_SET2_PHYSICAL_GLOVES:
			return {13046, 13058, 13056};
		case ExceedEnchantSkillSetType::RANK1_SET2_PHYSICAL_PANTS:
			return {13075, 13064, 13069};
		case ExceedEnchantSkillSetType::RANK1_SET2_PHYSICAL_SHOES:
			return {13121, 13114, 13119};
		case ExceedEnchantSkillSetType::RANK1_SET2_PHYSICAL_SHOULDER:
			return {13104, 13094, 13102};
		case ExceedEnchantSkillSetType::RANK1_SET2_PHYSICAL_TORSO:
			return {13144, 13135, 13133};
		case ExceedEnchantSkillSetType::RANK1_SET2_PHYSICAL_WEAPON:
			return {13029, 13006, 13023};
		case ExceedEnchantSkillSetType::RANK1_SET3_MAGICAL_WEAPON:
			return {13031, 13022, 13026};
		case ExceedEnchantSkillSetType::RANK1_SET3_PHYSICAL_WEAPON:
			return {13031, 13022, 13026};
		case ExceedEnchantSkillSetType::RANK2_SET1_MAGICAL_GLOVES:
			return {13050, 13047, 13057};
		case ExceedEnchantSkillSetType::RANK2_SET1_MAGICAL_PANTS:
			return {13072, 13075, 13068};
		case ExceedEnchantSkillSetType::RANK2_SET1_MAGICAL_SHOES:
			return {13125, 13122, 13120};
		case ExceedEnchantSkillSetType::RANK2_SET1_MAGICAL_SHOULDER:
			return {13088, 13105, 13103};
		case ExceedEnchantSkillSetType::RANK2_SET1_MAGICAL_TORSO:
			return {13139, 13145, 13134};
		case ExceedEnchantSkillSetType::RANK2_SET1_MAGICAL_WEAPON:
			return {13008, 13010, 13024};
		case ExceedEnchantSkillSetType::RANK2_SET1_PHYSICAL_GLOVES:
			return {13050, 13047, 13057};
		case ExceedEnchantSkillSetType::RANK2_SET1_PHYSICAL_PANTS:
			return {13072, 13075, 13070};
		case ExceedEnchantSkillSetType::RANK2_SET1_PHYSICAL_SHOES:
			return {13125, 13122, 13120};
		case ExceedEnchantSkillSetType::RANK2_SET1_PHYSICAL_SHOULDER:
			return {13091, 13105, 13103};
		case ExceedEnchantSkillSetType::RANK2_SET1_PHYSICAL_TORSO:
			return {13139, 13145, 13134};
		case ExceedEnchantSkillSetType::RANK2_SET2_MAGICAL_WEAPON:
			return {13010, 13032, 13004};
		case ExceedEnchantSkillSetType::RANK2_SET2_PHYSICAL_GLOVES:
			return {13050, 13043, 13059};
		case ExceedEnchantSkillSetType::RANK2_SET2_PHYSICAL_PANTS:
			return {13072, 13078, 13065};
		case ExceedEnchantSkillSetType::RANK2_SET2_PHYSICAL_SHOES:
			return {13125, 13109, 13115};
		case ExceedEnchantSkillSetType::RANK2_SET2_PHYSICAL_SHOULDER:
			return {13091, 13099, 13095};
		case ExceedEnchantSkillSetType::RANK2_SET2_PHYSICAL_TORSO:
			return {13139, 13129, 13136};
		case ExceedEnchantSkillSetType::RANK2_SET2_PHYSICAL_WEAPON:
			return {13010, 13032, 13007};
		case ExceedEnchantSkillSetType::RANK2_SET1_PHYSICAL_WEAPON:
			return {13008, 13010, 13024};
		case ExceedEnchantSkillSetType::RANK2_SET3_MAGICAL_WEAPON:
			return {13008, 13013, 13030};
		case ExceedEnchantSkillSetType::RANK2_SET3_PHYSICAL_WEAPON:
			return {13008, 13013, 13030};
		case ExceedEnchantSkillSetType::RANK3_SET1_MAGICAL_GLOVES:
			return {13038, 13051, 13060};
		case ExceedEnchantSkillSetType::RANK3_SET1_MAGICAL_PANTS:
			return {13080, 13073, 13063};
		case ExceedEnchantSkillSetType::RANK3_SET1_MAGICAL_SHOES:
			return {13112, 13126, 13116};
		case ExceedEnchantSkillSetType::RANK3_SET1_MAGICAL_SHOULDER:
			return {13082, 13089, 13096};
		case ExceedEnchantSkillSetType::RANK3_SET1_MAGICAL_TORSO:
			return {13142, 13140, 13137};
		case ExceedEnchantSkillSetType::RANK3_SET1_MAGICAL_WEAPON:
			return {13034, 13018, 13009};
		case ExceedEnchantSkillSetType::RANK3_SET1_PHYSICAL_GLOVES:
			return {13040, 13051, 13060};
		case ExceedEnchantSkillSetType::RANK3_SET1_PHYSICAL_PANTS:
			return {13080, 13073, 13066};
		case ExceedEnchantSkillSetType::RANK3_SET1_PHYSICAL_SHOES:
			return {13112, 13126, 13116};
		case ExceedEnchantSkillSetType::RANK3_SET1_PHYSICAL_SHOULDER:
			return {13084, 13092, 13096};
		case ExceedEnchantSkillSetType::RANK3_SET1_PHYSICAL_TORSO:
			return {13142, 13140, 13137};
		case ExceedEnchantSkillSetType::RANK3_SET1_PHYSICAL_WEAPON:
			return {13036, 13020, 13009};
		case ExceedEnchantSkillSetType::RANK3_SET2_MAGICAL_GLOVES:
			return {13038, 13048, 13044};
		case ExceedEnchantSkillSetType::RANK3_SET2_MAGICAL_PANTS:
			return {13080, 13076, 13079};
		case ExceedEnchantSkillSetType::RANK3_SET2_MAGICAL_SHOES:
			return {13112, 13123, 13110};
		case ExceedEnchantSkillSetType::RANK3_SET2_MAGICAL_SHOULDER:
			return {13082, 13106, 13100};
		case ExceedEnchantSkillSetType::RANK3_SET2_MAGICAL_TORSO:
			return {13142, 13146, 13130};
		case ExceedEnchantSkillSetType::RANK3_SET2_MAGICAL_WEAPON:
			return {13034, 13014, 13025};
		case ExceedEnchantSkillSetType::RANK3_SET2_PHYSICAL_GLOVES:
			return {13040, 13048, 13044};
		case ExceedEnchantSkillSetType::RANK3_SET2_PHYSICAL_PANTS:
			return {13080, 13076, 13079};
		case ExceedEnchantSkillSetType::RANK3_SET2_PHYSICAL_SHOES:
			return {13112, 13123, 13110};
		case ExceedEnchantSkillSetType::RANK3_SET2_PHYSICAL_SHOULDER:
			return {13084, 13106, 13100};
		case ExceedEnchantSkillSetType::RANK3_SET2_PHYSICAL_TORSO:
			return {13142, 13146, 13130};
		case ExceedEnchantSkillSetType::RANK3_SET2_PHYSICAL_WEAPON:
			return {13036, 13014, 13025};
		case ExceedEnchantSkillSetType::RANK3_SET3_MAGICAL_WEAPON:
			return {13018, 13014, 13033};
		case ExceedEnchantSkillSetType::RANK3_SET3_PHYSICAL_WEAPON:
			return {13020, 13014, 13033};
		case ExceedEnchantSkillSetType::RANK2_SET2_MAGICAL_GLOVES:
			return {13050, 13043, 13059};
		case ExceedEnchantSkillSetType::RANK2_SET2_MAGICAL_PANTS:
			return {13072, 13078, 13062};
		case ExceedEnchantSkillSetType::RANK2_SET2_MAGICAL_SHOES:
			return {13125, 13109, 13115};
		case ExceedEnchantSkillSetType::RANK2_SET2_MAGICAL_SHOULDER:
			return {13088, 13099, 13095};
		case ExceedEnchantSkillSetType::RANK2_SET2_MAGICAL_TORSO:
			return {13139, 13129, 13136};
		case ExceedEnchantSkillSetType::RANK4_SET1_MAGICAL_GLOVES:
			return {13053, 13039, 13045};
		case ExceedEnchantSkillSetType::RANK4_SET1_MAGICAL_PANTS:
			return {13077, 13081, 13079};
		case ExceedEnchantSkillSetType::RANK4_SET1_MAGICAL_SHOES:
			return {13117, 13113, 13111};
		case ExceedEnchantSkillSetType::RANK4_SET1_MAGICAL_SHOULDER:
			return {13086, 13083, 13101};
		case ExceedEnchantSkillSetType::RANK4_SET1_MAGICAL_TORSO:
			return {13138, 13143, 13131};
		case ExceedEnchantSkillSetType::RANK4_SET1_MAGICAL_WEAPON:
			return {13015, 13028, 13017};
		case ExceedEnchantSkillSetType::RANK4_SET1_PHYSICAL_GLOVES:
			return {13054, 13041, 13045};
		case ExceedEnchantSkillSetType::RANK4_SET1_PHYSICAL_PANTS:
			return {13077, 13081, 13079};
		case ExceedEnchantSkillSetType::RANK4_SET1_PHYSICAL_SHOES:
			return {13117, 13113, 13111};
		case ExceedEnchantSkillSetType::RANK4_SET1_PHYSICAL_SHOULDER:
			return {13087, 13085, 13101};
		case ExceedEnchantSkillSetType::RANK4_SET1_PHYSICAL_TORSO:
			return {13138, 13143, 13131};
		case ExceedEnchantSkillSetType::RANK4_SET1_PHYSICAL_WEAPON:
			return {13016, 13028, 13017};
		case ExceedEnchantSkillSetType::RANK4_SET2_MAGICAL_GLOVES:
			return {13053, 13052, 13049};
		case ExceedEnchantSkillSetType::RANK4_SET2_MAGICAL_PANTS:
			return {13077, 13074, 13076};
		case ExceedEnchantSkillSetType::RANK4_SET2_MAGICAL_SHOES:
			return {13117, 13127, 13124};
		case ExceedEnchantSkillSetType::RANK4_SET2_MAGICAL_SHOULDER:
			return {13086, 13090, 13107};
		case ExceedEnchantSkillSetType::RANK4_SET2_MAGICAL_TORSO:
			return {13138, 13141, 13147};
		case ExceedEnchantSkillSetType::RANK4_SET2_MAGICAL_WEAPON:
			return {13019, 13017, 13005};
		case ExceedEnchantSkillSetType::RANK4_SET2_PHYSICAL_GLOVES:
			return {13054, 13052, 13049};
		case ExceedEnchantSkillSetType::RANK4_SET2_PHYSICAL_PANTS:
			return {13077, 13074, 13076};
		case ExceedEnchantSkillSetType::RANK4_SET2_PHYSICAL_SHOES:
			return {13117, 13127, 13124};
		case ExceedEnchantSkillSetType::RANK4_SET2_PHYSICAL_SHOULDER:
			return {13087, 13093, 13107};
		case ExceedEnchantSkillSetType::RANK4_SET2_PHYSICAL_TORSO:
			return {13138, 13141, 13147};
		case ExceedEnchantSkillSetType::RANK4_SET2_PHYSICAL_WEAPON:
			return {13021, 13017, 13005};
		case ExceedEnchantSkillSetType::RANK4_SET3_MAGICAL_WEAPON:
			return {13035, 13001, 13005};
		case ExceedEnchantSkillSetType::RANK4_SET3_PHYSICAL_WEAPON:
			return {13037, 13001, 13005};
		case ExceedEnchantSkillSetType::RANK5_SET1_MAGICAL_TORSO:
			return {13235, 13236, 13238};
		case ExceedEnchantSkillSetType::RANK5_SET1_MAGICAL_GLOVES:
			return {13248, 13253, 13251};
		case ExceedEnchantSkillSetType::RANK5_SET1_MAGICAL_PANTS:
			return {13241, 13079, 13240};
		case ExceedEnchantSkillSetType::RANK5_SET1_MAGICAL_SHOULDER:
			return {13269, 13279, 13247};
		case ExceedEnchantSkillSetType::RANK5_SET1_MAGICAL_SHOES:
			return {13245, 13266, 13246};
		case ExceedEnchantSkillSetType::RANK5_SET1_PHYSICAL_TORSO:
			return {13235, 13236, 13238};
		case ExceedEnchantSkillSetType::RANK5_SET1_PHYSICAL_GLOVES:
			return {13251, 13249, 13248};
		case ExceedEnchantSkillSetType::RANK5_SET1_PHYSICAL_SHOULDER:
			return {13270, 13247, 13269};
		case ExceedEnchantSkillSetType::RANK5_SET1_PHYSICAL_PANTS:
			return {13241, 13079, 13240};
		case ExceedEnchantSkillSetType::RANK5_SET1_PHYSICAL_SHOES:
			return {13245, 13244, 13266};
		case ExceedEnchantSkillSetType::RANK5_SET1_MAGICAL_WEAPON:
			return {13228, 13234, 13231};
		case ExceedEnchantSkillSetType::RANK5_SET1_PHYSICAL_WEAPON:
			return {13229, 13234, 13231};
	}
	// the Java switch expression covers every constant (it would not compile otherwise)
	throw runtime::IllegalStateException("Unexpected value: " + std::string(xml::enumName(type)));
}

} // namespace

bool EnchantService::breakItem(model::gameobjects::player::Player& player, model::gameobjects::Item& targetItem, model::gameobjects::Item& parentItem) {
	model::items::storage::Storage& inventory = player.getInventory();
	if (!inventory.getItemByObjId(targetItem.getObjectId()) || !inventory.getItemByObjId(parentItem.getObjectId()))
		return false;

	const ItemTemplate* itemTemplate = targetItem.getItemTemplate();
	if (!itemTemplate->isArmor() && !itemTemplate->isWeapon()) {
		AuditLogger::log(player, "tried to break down incompatible item type");
		return false;
	}

	const ItemQuality itemQuality = qualityOf(*itemTemplate); // Java: the switch of calculateEffectiveLevel throws for a null quality
	int32_t effectiveLevel = calculateEffectiveLevel(itemQuality, itemTemplate->getLevel());
	if (effectiveLevel == 0)
		throw runtime::IllegalArgumentException("Invalid item quality for breaking item " + std::string(xml::enumName(itemQuality)));

	int32_t rndEffectiveLevel = javaAdd(effectiveLevel, Rnd::get(0, 10));
	if (itemTemplate->isWeapon())
		rndEffectiveLevel = javaAdd(rndEffectiveLevel, 5);

	// Omega Stones are limited to Drops
	int32_t stoneId;
	if (rndEffectiveLevel >= calculateEffectiveLevel(EnchantmentStone::EPSILON))
		stoneId = 166000195;
	else if (rndEffectiveLevel >= calculateEffectiveLevel(EnchantmentStone::DELTA))
		stoneId = 166000194;
	else if (rndEffectiveLevel >= calculateEffectiveLevel(EnchantmentStone::GAMMA))
		stoneId = 166000193;
	else if (rndEffectiveLevel >= calculateEffectiveLevel(EnchantmentStone::BETA))
		stoneId = 166000192;
	else
		stoneId = 166000191; // Alpha

	if (!inventory.decreaseByObjectId(parentItem.getObjectId(), 1) || !inventory.delete_(targetItem)) {
		AuditLogger::log(player, "possibly used break item hack");
		return false;
	}
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_DECOMPOSE_ITEM_SUCCEED(targetItem.getL10n()));
	item::ItemService::addItem(player, stoneId, itemTemplate->isWeapon() ? Rnd::get(2, 5) : Rnd::get(1, 3));
	return true;
}

int32_t EnchantService::calculateEffectiveLevel(model::enchants::EnchantmentStone enchantmentStone) {
	return calculateEffectiveLevel(model::enchants::getBaseQuality(enchantmentStone), model::enchants::getBaseLevel(enchantmentStone));
}

int32_t EnchantService::calculateEffectiveLevel(model::templates::item::ItemQuality itemQuality, int32_t itemLevel) {
	switch (itemQuality) {
		case ItemQuality::COMMON: // same as rare, since there's no EnchantmentStone enum having COMMON as a base
		case ItemQuality::RARE:
			return javaAdd(itemLevel, 5);
		case ItemQuality::LEGEND:
			return javaAdd(itemLevel, 10);
		case ItemQuality::UNIQUE:
			return javaAdd(itemLevel, 15);
		case ItemQuality::EPIC:
			return javaAdd(itemLevel, 20);
		case ItemQuality::MYTHIC:
			return javaAdd(itemLevel, 25);
		default:
			return 0;
	}
}

bool EnchantService::enchantItem(model::gameobjects::player::Player& player, model::gameobjects::Item& enchantmentStoneItem, model::gameobjects::Item& targetItem, runtime::Ptr<model::gameobjects::Item> supplementItem) {
	float successChance;

	if (targetItem.isAmplified())
		successChance = rate(player, RatesConfig::ENCHANTMENT_STONE_AMPLIFIED_CHANCES);
	else {
		successChance = rate(player, RatesConfig::ENCHANTMENT_STONE_BASE_CHANCES);

		EnchantmentStone enchantmentStone = model::enchants::getByItemId(enchantmentStoneItem.getItemId());
		int32_t itemLevel = targetItem.getItemTemplate()->getLevel();
		if (itemLevel < model::enchants::getBaseLevel(EnchantmentStone::ALPHA)) // ensure low lvl items don't get too high success chances
			itemLevel = model::enchants::getBaseLevel(EnchantmentStone::ALPHA);
		int32_t stoneToItemLevelDiff = javaSub(model::enchants::getBaseLevel(enchantmentStone), itemLevel);
		int32_t stoneToItemQualityDiff = model::templates::item::getQualityId(model::enchants::getBaseQuality(enchantmentStone)) -
			model::templates::item::getQualityId(qualityOf(*targetItem.getItemTemplate()));

		successChance += static_cast<float>(stoneToItemLevelDiff); // absolutely increase/reduce chance by 1% for every level difference
		successChance += static_cast<float>(stoneToItemQualityDiff * 5); // absolutely increase/reduce chance by 5% for each quality difference

		if (targetItem.getEnchantLevel() == 0) // boost enchant chance for +1 by 20%
			successChance *= 1.2f;
		else if (targetItem.getEnchantLevel() < 5) // boost enchant chance up to +5 by 10%
			successChance *= 1.1f;
		else if (targetItem.getEnchantLevel() >= 10) // reduce enchant chance from +10 by 10%
			successChance *= 0.9f;

		// Retail Tests: 80% = Success Cap for Enchanting without Supplements
		if (successChance >= 80)
			successChance = 80;

		// Supplement is used
		if (supplementItem) {
			// Amount of supplement items
			int32_t supplementUseCount = 1;
			// Additional success rate for the supplement
			const ItemTemplate* supplementTemplate = supplementItem->getItemTemplate();

			const EnchantItemAction* action = enchantActionOf(*supplementTemplate);
			if (action != nullptr) {
				if (action->isManastoneOnly())
					return false;
				// Add success rate of the supplement to the overall chance
				successChance += action->getChance();
			}

			action = enchantActionOf(*enchantmentStoneItem.getItemTemplate());
			if (action != nullptr)
				supplementUseCount = action->getCount();

			// Beginning from enchanting to +11, there are 2 times more supplements required
			if (targetItem.getEnchantLevel() >= 10)
				supplementUseCount = javaMul(supplementUseCount, 2);

			// Check the required amount of the supplements
			if (player.getInventory().getItemCountByItemId(supplementTemplate->getTemplateId()) < supplementUseCount)
				return false;

			// Put supplements to wait for update
			player.subtractSupplements(supplementUseCount, supplementTemplate->getTemplateId());

			// Success can't be higher than 95%
			if (successChance >= 95)
				successChance = 95;
		}
	}

	bool result = Rnd::chance() < successChance;

	if (player.hasAccess(configs::administration::AdminConfig::ENCHANT_INFO.load()))
		PacketSendUtility::sendMessage(player, enchantInfo(result, successChance));

	return result;
}

void EnchantService::enchantItemAct(model::gameobjects::player::Player& player, model::gameobjects::Item& parentItem, model::gameobjects::Item& targetItem, model::gameobjects::Item& /*supplementItem*/, int32_t currentEnchant, bool success) {
	// Java never reads supplementItem here (EnchantService.java:173-231; header request m5c-h05 makes it a nullable Ptr)
	int32_t addLevel = 1;

	int32_t maxEnchant = targetItem.getItemTemplate()->getMaxEnchantLevel(); // max enchant level from item_templates
	maxEnchant = javaAdd(maxEnchant, targetItem.getEnchantBonus());
	if (targetItem.getEnchantLevel() < maxEnchant) {
		float chance = Rnd::chance(); // crit modifier
		if (chance < 5)
			addLevel = 3;
		else if (chance < 10)
			addLevel = 2;
	}

	if (!player.getInventory().decreaseByObjectId(parentItem.getObjectId(), 1)) {
		AuditLogger::log(player, "possibly used enchant hack");
		return;
	}
	// Decrease required supplements
	player.updateSupplements();

	// Items that are Fabled or Eternal can get up to +15.
	if (success) {
		if (!targetItem.isAmplified() && javaAdd(currentEnchant, addLevel) > maxEnchant)
			currentEnchant = maxEnchant;
		else
			currentEnchant = javaAdd(currentEnchant, addLevel);
	} else {
		// Retail: http://powerwiki.na.aiononline.com/aion/Patch+Notes:+1.9.0.1
		// When socketing fails at +11~+15, the value falls back to +10.
		if (targetItem.isAmplified()) {
			currentEnchant = maxEnchant;
			targetItem.setAmplified(false);
		} else if ((currentEnchant > 10 && maxEnchant > 10)) {
			currentEnchant = 10;
		} else if (currentEnchant > 0) {
			currentEnchant -= 1;
		}
	}

	if (targetItem.isAmplified())
		maxEnchant = 255;

	setEnchantLevel(player, targetItem, std::min(currentEnchant, maxEnchant));

	if (success)
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ENCHANT_ITEM_SUCCEED_NEW(targetItem.getL10n(), targetItem.getEnchantLevel()));
	else {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ENCHANT_ITEM_FAILED(targetItem.getL10n()));
		if (targetItem.getItemTemplate()->getEnchantType() > 0) {
			if (targetItem.isEquipped())
				player.getEquipment().decreaseEquippedItemCount(targetItem.getObjectId(), 1);
			else
				player.getInventory().decreaseByObjectId(targetItem.getObjectId(), 1);
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ENCHANT_TYPE1_ENCHANT_FAIL(targetItem.getL10n()));
		} else {
			targetItem.removeRemainingTuningCountIfPossible();
		}
	}
}

void EnchantService::setEnchantLevel(model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t enchantLevel) {
	item.setEnchantLevel(enchantLevel);
	int32_t oldBuffId = item.getBuffSkill();
	int32_t newBuffId = 0;
	if (enchantLevel >= 20) {
		// The breakthrough skill is granted once at +20 and retained through subsequent enchantments.
		newBuffId = oldBuffId != 0 ? oldBuffId : getEquipBuff(item);
	}
	if (newBuffId != oldBuffId) {
		item.setBuffSkill(newBuffId);
		if (item.isEquipped()) {
			if (oldBuffId != 0)
				SkillLearnService::removeSkill(player, oldBuffId);
			if (newBuffId != 0)
				SkillLearnService::learnTemporarySkill(player, newBuffId, 1);
		}
		if (newBuffId != 0) {
			const skillengine::model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(newBuffId);
			if (skillTemplate == nullptr) // Java: DataManager.SKILL_DATA.getSkillTemplate(newBuffId).getL10n()
				throw runtime::NullPointerException("SKILL_DATA.getSkillTemplate(" + std::to_string(newBuffId) + ")");
			std::string skillName = skillTemplate->getL10n();
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_SKILL_ENCHANT(item.getL10n(), enchantLevel, skillName));
			if (!item.isEquipped())
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_SKILL_ABLE_EQUIPED(item.getL10n(), skillName));
		} else {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_SKILL_DELETE(item.getL10n()));
		}
	}
	if (Ptr<model::enchants::EnchantEffect> enchantEffect = item.getEnchantEffect()) {
		enchantEffect->endEffect(player);
		item.setEnchantEffect(nullptr);
	}
	if (item.isEquipped()) {
		player.getGameStats()->updateStatsVisually();
		if (enchantLevel > 0)
			applyEnchantEffect(item, player, enchantLevel);
	}

	item::ItemPacketService::updateItemAfterInfoChange(player, item, item::ItemPacketService::ItemUpdateType::STATS_CHANGE);
	if (item.isEquipped())
		player.getEquipment().setPersistentState(Persistable_PersistentState::UPDATE_REQUIRED);
	else
		player.getInventory().setPersistentState(Persistable_PersistentState::UPDATE_REQUIRED);
}

void EnchantService::applyEnchantEffect(model::gameobjects::Item& targetItem, model::gameobjects::player::Player& owner, int32_t enchantLevel) {
	const dataholders::EnchantData::LevelStats* enchant = dataholders::DataManager::ENCHANT_DATA->getTemplates(*targetItem.getItemTemplate());
	if (enchant == nullptr)
		return;
	// Java: enchant.keySet().stream().mapToInt(Integer::intValue).max().getAsInt() (NoSuchElementException for an empty map)
	if (enchant->empty())
		throw runtime::NoSuchElementException("No value present");
	int32_t maxTemplateLevel = std::max_element(enchant->begin(), enchant->end(), [](const auto& a, const auto& b) { return a.first < b.first; })->first;
	// Java Map.get: null for a missing level
	auto statsAt = [enchant](int32_t level) -> const std::vector<model::enchants::EnchantStat>* {
		auto found = enchant->find(level);
		return found == enchant->end() ? nullptr : found->second;
	};
	// Java List<EnchantStat> stats, nullable: the direct arm hands Map.get's result to new EnchantEffect, whose for over it throws for null
	std::optional<std::vector<const model::enchants::EnchantStat*>> stats;
	if (enchantLevel > maxTemplateLevel && maxTemplateLevel < 21) // usually only test templates have max level < 21
		throw runtime::IllegalArgumentException("Missing bonus stats for +" + std::to_string(enchantLevel) + " (item:" +
			std::to_string(targetItem.getItemId()) + ") in enchant templates");
	else if (enchantLevel < maxTemplateLevel) {
		if (const std::vector<model::enchants::EnchantStat>* levelStats = statsAt(enchantLevel)) {
			stats.emplace();
			for (const model::enchants::EnchantStat& stat : *levelStats)
				stats->push_back(&stat);
		}
	} else {
		// maxTemplateLevel - 1 (second to last template entry) = maximum stats
		const std::vector<model::enchants::EnchantStat>* maximumStats = statsAt(javaSub(maxTemplateLevel, 1));
		if (maximumStats == nullptr) // Java: new ArrayList<>(null)
			throw runtime::NullPointerException("enchant.get(" + std::to_string(javaSub(maxTemplateLevel, 1)) + ")");
		stats.emplace();
		for (const model::enchants::EnchantStat& stat : *maximumStats)
			stats->push_back(&stat);
		// maxTemplateLevel (last template entry) = bonus stats per level above max
		const std::vector<model::enchants::EnchantStat>* limitlessBoni = statsAt(maxTemplateLevel);
		for (int32_t i = 0; i <= javaSub(enchantLevel, maxTemplateLevel); i++) {
			if (limitlessBoni == nullptr) // Java: stats.addAll(null)
				throw runtime::NullPointerException("enchant.get(" + std::to_string(maxTemplateLevel) + ")");
			for (const model::enchants::EnchantStat& stat : *limitlessBoni)
				stats->push_back(&stat);
		}
	}
	if (Ptr<model::enchants::EnchantEffect> enchantEffect = targetItem.getEnchantEffect())
		enchantEffect->endEffect(owner);
	if (!stats) // Java: new EnchantEffect(targetItem, owner, null) iterates the null list
		throw runtime::NullPointerException("enchant.get(" + std::to_string(enchantLevel) + ")");
	Ref<model::enchants::EnchantEffect> effect = model::enchants::EnchantEffect::create(targetItem, owner, *stats);
	targetItem.setEnchantEffect(Ptr<model::enchants::EnchantEffect>(effect));
}

bool EnchantService::socketManastone(model::gameobjects::player::Player& player, model::gameobjects::Item& manastone, model::gameobjects::Item& targetItem, runtime::Ptr<model::gameobjects::Item> supplementItem, int32_t fusionedWeaponLevel) {
	int32_t targetItemLevel;

	// Fusioned weapon. Primary weapon level.
	if (fusionedWeaponLevel == 1)
		targetItemLevel = targetItem.getItemTemplate()->getLevel();
	// Fusioned weapon. Secondary weapon level.
	else
		targetItemLevel = fusionedTemplateOf(targetItem).getLevel();

	int32_t stoneLevel = manastone.getItemTemplate()->getLevel();
	int32_t slotLevel = JavaFloat::doubleToInt(10 * std::ceil(static_cast<double>(javaAdd(targetItemLevel, 10)) / 10.0));

	// The current amount of socketed stones
	int32_t stoneCount;

	// Manastone level shouldn't be greater as 20 + item level
	// Example: item level: 1 - 10. Manastone level should be <= 20
	if (stoneLevel > slotLevel)
		return false;

	// Fusioned weapon. Primary weapon slots.
	if (fusionedWeaponLevel == 1)
		// Count the inserted stones in the primary weapon
		stoneCount = targetItem.getItemStones()->size();
	// Fusioned weapon. Secondary weapon slots.
	else
		// Count the inserted stones in the secondary weapon
		stoneCount = targetItem.getFusionStones()->size();

	// Fusioned weapon. Primary weapon slots.
	if (fusionedWeaponLevel == 1) {
		// Find all free slots in the primary weapon
		if (stoneCount >= targetItem.getSockets(false)) {
			AuditLogger::log(player, "Manastone socket overload");
			return false;
		}
	}
	// Fusioned weapon. Secondary weapon slots.
	else if (!targetItem.hasFusionedItem() || stoneCount >= targetItem.getSockets(true)) {
		// Find all free slots in the secondary weapon
		AuditLogger::log(player, "Manastone socket overload");
		return false;
	}

	// Start value of success
	float successChance = rate(player, RatesConfig::MANASTONE_CHANCES);

	if (model::templates::item::getQualityId(qualityOf(*manastone.getItemTemplate())) >= model::templates::item::getQualityId(ItemQuality::RARE))
		successChance *= 0.8f;

	// Next socket difficulty modifier
	float socketDiff = static_cast<float>(stoneCount) * 1.25f + 1.75f;

	// Level difference
	successChance += static_cast<float>(javaSub(slotLevel, stoneLevel)) / socketDiff;

	// The supplement item is used
	if (supplementItem) {
		int32_t supplementUseCount = 0;
		const ItemTemplate* manastoneTemplate = manastone.getItemTemplate();

		int32_t manastoneCount;
		// Not fusioned
		if (fusionedWeaponLevel == 1)
			manastoneCount = javaAdd(targetItem.getItemStones()->size(), 1);
		// Fusioned
		else
			manastoneCount = javaAdd(targetItem.getFusionStones()->size(), 1);

		// Additional success rate for the supplement
		const ItemTemplate* supplementTemplate = supplementItem->getItemTemplate();

		bool isManastoneOnly = false;
		const EnchantItemAction* action = enchantActionOf(*manastoneTemplate);
		if (action != nullptr)
			supplementUseCount = action->getCount();

		action = enchantActionOf(*supplementTemplate);
		if (action != nullptr) {
			// Add successRate
			successChance += action->getChance();
			isManastoneOnly = action->isManastoneOnly();
		}

		if (isManastoneOnly)
			supplementUseCount = 1;
		else if (stoneCount > 0)
			supplementUseCount = javaMul(supplementUseCount, manastoneCount);

		if (player.getInventory().getItemCountByItemId(supplementTemplate->getTemplateId()) < supplementUseCount)
			return false;

		// Put up supplements to wait for update
		player.subtractSupplements(supplementUseCount, supplementTemplate->getTemplateId());
	}

	bool result = Rnd::chance() < successChance;

	// For test purpose. To use by administrator
	if (player.hasAccess(configs::administration::AdminConfig::ENCHANT_INFO.load()))
		PacketSendUtility::sendMessage(player, enchantInfo(result, successChance));

	return result;
}

bool EnchantService::socketManastoneAct(model::gameobjects::player::Player& player, model::gameobjects::Item& parentItem, model::gameobjects::Item& targetItem, model::gameobjects::Item& /*supplementItem*/, int32_t targetWeapon, bool result) {
	// Java never reads supplementItem here (EnchantService.java:404-424; header request m5c-h05 makes it a nullable Ptr)
	if (!player.getInventory().decreaseByObjectId(parentItem.getObjectId(), 1))
		return false;
	// Decrease required supplements
	player.updateSupplements();
	if (result) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_SUCCEED(targetItem.getL10n()));

		Ptr<model::items::ManaStone> manaStone =
			item::ItemSocketService::addManaStone(Ptr<Item>(targetItem), parentItem.getItemTemplate()->getTemplateId(), targetWeapon != 1);
		if (targetItem.isEquipped()) {
			model::stats::listeners::ItemEquipmentListener::addStoneStats(targetItem, manaStone, *player.getGameStats());
			player.getGameStats()->updateStatsAndSpeedVisually();
		}
	} else {
		targetItem.removeRemainingTuningCountIfPossible();
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GIVE_ITEM_OPTION_FAILED(targetItem.getL10n()));
	}

	item::ItemPacketService::updateItemAfterInfoChange(player, targetItem, item::ItemPacketService::ItemUpdateType::STATS_CHANGE);
	return true;
}

int32_t EnchantService::getEquipBuff(model::gameobjects::Item& item) {
	std::optional<ExceedEnchantSkillSetType> exceedEnchantSkill = item.getItemTemplate()->getExceedEnchantSkill();
	if (!exceedEnchantSkill) // case null -> new int[] {0}
		return Rnd::get(std::array<int32_t, 1>{0});
	return Rnd::get(exceedEnchantSkills(*exceedEnchantSkill));
}

void EnchantService::amplifyItem(runtime::Ptr<model::gameobjects::player::Player> player, int32_t targetItemObjId, int32_t materialId, int32_t toolId) {
	if (!player)
		return;
	Ptr<Item> targetItem = player->getEquipment().getEquippedItemByObjId(targetItemObjId);

	if (!targetItem)
		targetItem = player->getInventory().getItemByObjId(targetItemObjId);

	Ptr<Item> material = player->getInventory().getItemByObjId(materialId);
	Ptr<Item> tool = player->getInventory().getItemByObjId(toolId);

	if (!targetItem || !material || !tool) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_NO_TARGET_ITEM());
		return;
	}
	if (targetItem->isAmplified()) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_ALREADY());
		return;
	}
	if (!targetItem->getItemTemplate()->canExceedEnchant()) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_CANNOT_01(targetItem->getL10n()));
		return;
	}
	if (targetItem->getEnchantLevel() < targetItem->getMaxEnchantLevel()) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_CANNOT_02());
		return;
	}
	if (targetItem->getItemId() != material->getItemId() && material->getItemId() != 166500002 && material->getItemId() != 166500005) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_NO_TARGET_ITEM());
		return;
	}
	if (player->getInventory().decreaseByObjectId(material->getObjectId(), 1) && player->getInventory().decreaseByObjectId(tool->getObjectId(), 1)) {
		targetItem->setAmplified(true);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_EXCEED_SUCCEED(targetItem->getL10n()));
		item::ItemPacketService::updateItemAfterInfoChange(*player, *targetItem);

		if (targetItem->isEquipped())
			player->getEquipment().setPersistentState(Persistable_PersistentState::UPDATE_REQUIRED);
		else
			player->getInventory().setPersistentState(Persistable_PersistentState::UPDATE_REQUIRED);
	}
}

} // namespace aion::gameserver::services
