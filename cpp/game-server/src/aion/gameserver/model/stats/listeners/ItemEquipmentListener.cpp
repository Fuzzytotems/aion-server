#include "aion/gameserver/model/stats/listeners/ItemEquipmentListener.h"

#include <cstdint>
#include <memory>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/enchants/TemperingEffect.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/ChargeInfo.h"
#include "aion/gameserver/model/items/IdianStone.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/items/RandomBonusEffect.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/model/templates/item/ItemAttackTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/WeaponStats.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/model/templates/itemset/FullBonus.h"
#include "aion/gameserver/model/templates/itemset/ItemSetTemplate.h"
#include "aion/gameserver/model/templates/itemset/PartBonus.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_COOLDOWN.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/EnchantService.h"
#include "aion/gameserver/services/SkillLearnService.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::stats::listeners {

using calc::StatOwner;
using calc::functions::IStatFunction;
using calc::functions::StatFunction;
using container::StatEnum;
using gameobjects::Item;
using gameobjects::player::Player;
using runtime::Ptr;
using runtime::Ref;

namespace {

using TemplateModifiers = std::vector<std::unique_ptr<StatFunction>>;

/** Java: a dereference of a null reference (NullPointerException) */
template <class T>
const T& nonNull(const T* value, const char* what) {
	if (value == nullptr)
		throw runtime::NullPointerException(std::string(what) + " is null");
	return *value;
}

/** Java List<StatFunction> of static data (null: an empty vector) */
std::vector<const StatFunction*> modifiersOf(const TemplateModifiers* modifiers) {
	std::vector<const StatFunction*> result;
	if (modifiers != nullptr) {
		for (const std::unique_ptr<StatFunction>& modifier : *modifiers)
			result.push_back(modifier.get());
	}
	return result;
}

/** The borrowed functions of a List<? extends IStatFunction> passed to CreatureGameStats.addEffect */
std::vector<Ptr<IStatFunction>> functionsOf(const std::vector<Ptr<StatFunction>>& modifiers) {
	return std::vector<Ptr<IStatFunction>>(modifiers.begin(), modifiers.end());
}

/** Java: addEffect(owner, list) of a static data list; a null list is Java's NullPointerException in addEffectOnly */
void addTemplateEffect(container::CreatureGameStats& cgs, StatOwner& owner, const TemplateModifiers* modifiers) {
	if (modifiers == nullptr)
		throw runtime::NullPointerException("modifiers is null");
	std::vector<Ptr<IStatFunction>> functions;
	for (const std::unique_ptr<StatFunction>& modifier : *modifiers)
		functions.emplace_back(StatFunction::ofTemplate(modifier.get()));
	cgs.addEffect(Ptr<StatOwner>(owner), functions);
}

/** Java Set<ManaStone> of an item (null: empty) in its TreeSet order */
std::vector<Ptr<items::ManaStone>> stonesOf(Ptr<runtime::RcTreeSet<Ref<items::ManaStone>>> stones) {
	if (!stones)
		return {};
	return stones->snapshot();
}

} // namespace

void ItemEquipmentListener::onItemEquipment(Item& item, Player& owner) {
	owner.getController().cancelUseItem();
	const templates::item::ItemTemplate& itemTemplate = nonNull(item.getItemTemplate(), "itemTemplate");

	addWeaponStats(item, *owner.getGameStats());

	if (itemTemplate.isItemSet())
		recalculateItemSet(itemTemplate.getItemSet(), owner);
	if (item.hasManaStones())
		addStonesStats(item, stonesOf(item.getItemStones()), *owner.getGameStats());
	if (item.hasFusionStones())
		addStonesStats(item, stonesOf(item.getFusionStones()), *owner.getGameStats());

	Ptr<items::IdianStone> idianStone = item.getIdianStone();
	if (idianStone)
		idianStone->onEquip(owner, item.getEquipmentSlot());

	if (item.getBuffSkill() != 0) {
		const skillengine::model::SkillTemplate& buffSkill =
			nonNull(dataholders::DataManager::SKILL_DATA->getSkillTemplate(item.getBuffSkill()), "SKILL_DATA.getSkillTemplate(buffSkill)");
		services::SkillLearnService::learnTemporarySkill(owner, item.getBuffSkill(), 1);
		int64_t currTime = commons::utils::currentTimeMillis();
		int64_t oldCooldown = owner.getSkillCoolDown(buffSkill.getCooldownId());
		int64_t newCooldown;
		if (oldCooldown - currTime > 15000) // cd active
			newCooldown = oldCooldown;
		else
			newCooldown = currTime + 15000;
		owner.setSkillCoolDown(buffSkill.getCooldownId(), newCooldown);
		utils::PacketSendUtility::sendPacket(owner, network::aion::serverpackets::SM_SKILL_COOLDOWN(buffSkill.getSkillId(), newCooldown));
	}
	forEachBonusStats([&owner](items::RandomBonusEffect& bonusStats) { bonusStats.applyEffect(owner); },
		{item.getBonusStatsEffect(), item.getFusionedItemBonusStatsEffect()});
	if (Ptr<items::ChargeInfo> conditioningInfo = item.getConditioningInfo()) {
		owner.getObserveController()->addObserver(*conditioningInfo);
		conditioningInfo->setPlayer(Ptr<Player>(owner));
	}
	if (item.getEnchantLevel() > 0)
		services::EnchantService::applyEnchantEffect(item, owner, item.getEnchantLevel());
	if (item.getTempering() > 0)
		enchants::TemperingEffect::apply(owner, item);
	owner.getGameStats()->updateArmorMasteryStats(owner.getEquipment().getEquippedItems());
}

void ItemEquipmentListener::forEachBonusStats(const std::function<void(items::RandomBonusEffect&)>& action,
	std::initializer_list<Ptr<items::RandomBonusEffect>> bonusStatsEffects) {
	for (const Ptr<items::RandomBonusEffect>& bonusStats : bonusStatsEffects)
		if (bonusStats)
			action(*bonusStats);
}

void ItemEquipmentListener::onItemUnequipment(Item& item, Player& owner) {
	AION_UNPORTED();
}

void ItemEquipmentListener::addWeaponStats(Item& item, container::CreatureGameStats& cgs) {
	const templates::item::ItemTemplate& itemTemplate = nonNull(item.getItemTemplate(), "itemTemplate");
	std::vector<const StatFunction*> mainWeaponModifiers = modifiersOf(itemTemplate.getModifiers());

	std::vector<Ptr<StatFunction>> modifiersToApply;
	std::vector<Ref<StatFunction>> createdModifiers; // keeps the run-time functions alive until the item and the stats hold them
	if ((item.getEquipmentSlot() & items::getSlotIdMask(items::ItemSlot::MAIN_OR_SUB)) != 0) {
		for (const StatFunction* modifier : extractApplicableWeaponModifiers(item, mainWeaponModifiers))
			modifiersToApply.push_back(StatFunction::ofTemplate(modifier));
		if (item.hasFusionedItem()) {
			// add all bonus modifiers according to rules
			const templates::item::ItemTemplate& fusionedItemTemplate = nonNull(item.getFusionedItemTemplate(), "fusionedItemTemplate");
			templates::item::enums::ItemGroup weaponType = fusionedItemTemplate.getItemGroup();
			const TemplateModifiers* fusionedItemModifiers = fusionedItemTemplate.getModifiers();
			if (fusionedItemModifiers != nullptr) {
				for (const StatFunction* modifier : extractApplicableWeaponModifiers(item, modifiersOf(fusionedItemModifiers)))
					modifiersToApply.push_back(StatFunction::ofTemplate(modifier));
			}

			// add 10% of Magic Boost and Attack
			const templates::item::WeaponStats* weaponStats = fusionedItemTemplate.getWeaponStats();
			if (weaponStats != nullptr) {
				int32_t boostMagicalSkill = templates::detail::floatToInt(0.1f * weaponStats->getBoostMagicalSkill());
				int32_t attack = templates::detail::floatToInt(0.1f * weaponStats->getMeanDamage());
				using templates::item::enums::ItemGroup;
				if (weaponType == ItemGroup::ORB || weaponType == ItemGroup::STAFF || weaponType == ItemGroup::SPELLBOOK || weaponType == ItemGroup::GUN
					|| weaponType == ItemGroup::CANNON || weaponType == ItemGroup::HARP || weaponType == ItemGroup::KEYBLADE) {
					createdModifiers.push_back(
						calc::functions::RcStatFunction<calc::functions::StatAddFunction>::create(StatEnum::BOOST_MAGICAL_SKILL, boostMagicalSkill, false));
					modifiersToApply.emplace_back(createdModifiers.back());
				}
				std::optional<templates::item::ItemAttackType> attackType = itemTemplate.getAttackType();
				if (!attackType)
					throw runtime::NullPointerException("attackType is null");
				createdModifiers.push_back(calc::functions::RcStatFunction<calc::functions::StatAddFunction>::create(
					templates::item::isMagical(*attackType) ? StatEnum::MAGICAL_ATTACK : StatEnum::PHYSICAL_ATTACK, attack, false));
				modifiersToApply.emplace_back(createdModifiers.back());
			}
		}
	} else {
		for (const StatFunction* modifier : mainWeaponModifiers)
			modifiersToApply.push_back(StatFunction::ofTemplate(modifier));
	}
	item.setCurrentModifiers(modifiersToApply);
	cgs.addEffect(Ptr<StatOwner>(item), functionsOf(modifiersToApply));
}

std::vector<const StatFunction*> ItemEquipmentListener::extractApplicableWeaponModifiers(Item& item, const std::vector<const StatFunction*>& modifiers) {
	std::vector<const StatFunction*> allModifiers;
	for (const StatFunction* modifier : modifiers) {
		switch (StatFunction::ofTemplate(modifier)->getName()) {
			case StatEnum::ATTACK_SPEED:
			case StatEnum::PVP_ATTACK_RATIO:
			case StatEnum::BOOST_CASTING_TIME:
				continue;
			default:
				allModifiers.push_back(modifier);
		}
	}
	return allModifiers;
}

void ItemEquipmentListener::recalculateItemSet(const templates::itemset::ItemSetTemplate* itemSetTemplate, Player& player) {
	if (itemSetTemplate == nullptr)
		return;

	// templates are owners by identity; the interface has no mutating member (StatOwner.h)
	auto& setOwner = const_cast<templates::itemset::ItemSetTemplate&>(*itemSetTemplate);
	// TODO quite
	player.getGameStats()->endEffect(setOwner);
	// 1.- Check equipment for items already equip with this itemSetTemplate id
	int32_t itemSetPartsEquipped = player.getEquipment().itemSetPartsEquipped(itemSetTemplate->getId());

	// 2.- Check Item Set Parts and add effects one by one if not done already
	for (const templates::itemset::PartBonus& itempartbonus : itemSetTemplate->getPartbonus())
		if (itempartbonus.getCount() <= itemSetPartsEquipped)
			addTemplateEffect(*player.getGameStats(), setOwner, itempartbonus.getModifiers());

	// 3.- Finally check if all items are applied and set the full bonus if not already applied
	const templates::itemset::FullBonus* fullbonus = itemSetTemplate->getFullbonus();
	if (fullbonus != nullptr && itemSetPartsEquipped == fullbonus->getCount()) {
		// Add the full bonus with index = total parts + 1 to avoid confusion with part bonus equal to number of
		// objects
		addTemplateEffect(*player.getGameStats(), setOwner, fullbonus->getModifiers());
	}
}

void ItemEquipmentListener::addStonesStats(Item& item, const std::vector<Ptr<items::ManaStone>>& itemStones, container::CreatureGameStats& cgs) {
	if (itemStones.empty())
		return;
	for (const Ptr<items::ManaStone>& stone : itemStones)
		addStoneStats(item, stone, cgs);
}

void ItemEquipmentListener::addStoneStats(Item& item, Ptr<items::ManaStone> stone, container::CreatureGameStats& cgs) {
	// C++: getModifiers() is never null (ManaStone.h), Java's null check has no counterpart
	if (!stone)
		return;
	std::vector<Ptr<IStatFunction>> functions;
	for (const StatFunction* modifier : stone->getModifiers())
		functions.emplace_back(StatFunction::ofTemplate(modifier));
	cgs.addEffect(Ptr<StatOwner>(*stone), functions);
}

void ItemEquipmentListener::removeStoneStats(const std::vector<Ptr<items::ManaStone>>& itemStones, container::CreatureGameStats& cgs) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::listeners
