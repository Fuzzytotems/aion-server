#include "aion/gameserver/services/StigmaService.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemQuality.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/SkillLearnService.h"
#include "aion/gameserver/services/trade/PricesService.h"
#include "aion/gameserver/skillengine/model/SkillLearnTemplate.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::services {

namespace {

/** Java `itemTemplate.getItemQuality()` followed by `.equals(...)`: a template without a quality is a NullPointerException */
model::templates::item::ItemQuality qualityOf(const model::templates::item::ItemTemplate& itemTemplate) {
	std::optional<model::templates::item::ItemQuality> quality = itemTemplate.getItemQuality();
	if (!quality)
		throw runtime::NullPointerException("itemQuality");
	return *quality;
}

/** Java String.equalsIgnoreCase (ASCII: stack names are) */
bool equalsIgnoreCase(std::string_view a, std::string_view b) {
	return std::ranges::equal(a, b, [](char x, char y) { return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y)); });
}

/** Java `skill.getSkillTemplate()` dereferenced: a skill entry without a template is a NullPointerException */
const skillengine::model::SkillTemplate& templateOf(model::skill::PlayerSkillEntry& skill) {
	const skillengine::model::SkillTemplate* skillTemplate = skill.getSkillTemplate();
	if (skillTemplate == nullptr)
		throw runtime::NullPointerException("skill template of " + std::to_string(skill.getSkillId()));
	return *skillTemplate;
}

/**
 * Java DataManager.SKILL_DATA.getSkillTemplatesByGroup(group), iterated: the C++ answers null for an unknown group (Java's map answers null too,
 * and iterating it is a NullPointerException)
 */
const std::vector<const skillengine::model::SkillTemplate*>& skillTemplatesByGroup(std::string_view skillGroup) {
	const std::vector<const skillengine::model::SkillTemplate*>* templates = dataholders::DataManager::SKILL_DATA->getSkillTemplatesByGroup(skillGroup);
	if (templates == nullptr)
		throw runtime::NullPointerException("no skill templates of group " + std::string(skillGroup));
	return *templates;
}

/**
 * Java: the anonymous ItemUseObserver of chargeStigma (StigmaService.java:427-437, fieldmap key StigmaService$1), stored in the player's
 * ObserveController until the task or abort() removes it. Its captures are the fieldmap's: the player, the charge stone, and the stigma's item
 * and object ids.
 */
struct StigmaService_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<model::gameobjects::player::Player> player; // captured param Player player
	const runtime::Ref<model::gameobjects::Item> chargeStone;      // captured param Item chargeStone
	const int32_t parentItemId;                                     // captured local final int parentItemId
	const int32_t parentObjectId;                                   // captured local final int parentObjectId

	static runtime::Ref<StigmaService_ItemUseObserver> create(model::gameobjects::player::Player& player, model::gameobjects::Item& chargeStone,
		int32_t parentItemId, int32_t parentObjectId) {
		return runtime::makeRef<StigmaService_ItemUseObserver>(player, chargeStone, parentItemId, parentObjectId);
	}

	void abort() override {
		player->getController().cancelTask(model::TaskId::ITEM_USE);
		utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_ITEM_CANCELED());
		utils::PacketSendUtility::broadcastPacket(*player,
			network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parentObjectId, chargeStone->getObjectId(), parentItemId, 0, 2, 0),
			true);
		player->getObserveController()->removeObserver(*this);
	}

protected:
	StigmaService_ItemUseObserver(model::gameobjects::player::Player& playerValue, model::gameobjects::Item& chargeStoneValue, int32_t parentItemIdValue,
		int32_t parentObjectIdValue)
		: player(runtime::Ref<model::gameobjects::player::Player>(playerValue)), chargeStone(runtime::Ref<model::gameobjects::Item>(chargeStoneValue)),
		  parentItemId(parentItemIdValue), parentObjectId(parentObjectIdValue) {}
	~StigmaService_ItemUseObserver() override = default;
};

} // namespace

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
// anonymous ItemUseObserver at StigmaService.java:376 (com.aionemu.gameserver.services.StigmaService$1); local observer; storage: stored in
// ObserveController
//   anonymous Runnable at StigmaService.java:388 (com.aionemu.gameserver.services.StigmaService$2); argument 1 of schedule(); storage: task

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.StigmaService");

bool StigmaService::notifyEquipAction(model::gameobjects::player::Player& player, model::gameobjects::Item& resultItem, int64_t slot) {
	if (resultItem.getItemTemplate()->isStigma()) {
		const model::templates::item::Stigma* stigmaInfo = resultItem.getItemTemplate()->getStigma();
		int32_t stigmaLevel = resultItem.getEnchantLevel();
		std::string stigmaName = commons::utils::StringUtils::toUpperCase(commons::utils::StringUtils::replace(resultItem.getItemName(), " ", ""));
		bool replace = false;
		for (const runtime::Ptr<model::gameobjects::Item>& i : player.getEquipment().getEquippedItemsAllStigma()) {
			if (i->getEquipmentSlot() == slot) {
				if (stigmaName != commons::utils::StringUtils::toUpperCase(commons::utils::StringUtils::replace(i->getItemName(), " ", "")))
					return false;
				removeStigmaSkills(player, i->getItemTemplate()->getStigma(), i->getEnchantLevel(), i->getEnchantLevel() > resultItem.getEnchantLevel());
				replace = true;
				break;
			}
		}
		if (!replace) {
			// check the number of stigma wearing
			if (model::items::isRegularStigma(slot)
				&& getPossibleStigmaCount(player) <= static_cast<int64_t>(player.getEquipment().getEquippedItemsRegularStigma().size())) {
				utils::audit::AuditLogger::log(player, "tried to equip stigma, exceeding the socket limit");
				return false;
			} else if (model::items::isAdvancedStigma(slot)
				&& getPossibleAdvancedStigmaCount(player) <= static_cast<int64_t>(player.getEquipment().getEquippedItemsAdvancedStigma().size())) {
				utils::audit::AuditLogger::log(player, "tried to equip advanced stigma, exceeding the socket limit");
				return false;
			}
		}

		int64_t kinahcount = 25000;
		// Sets the price for equipping stigma during mission in Space of Destiny [ID: 320070000] and Sliver of darkness [ID: 310070000]
		if ((player.getRace() == model::Race::ASMODIANS && player.getWorldId() == 320070000)
			|| (player.getRace() == model::Race::ELYOS && player.getWorldId() == 310070000))
			kinahcount = 1000;
		else if (qualityOf(*resultItem.getItemTemplate()) == model::templates::item::ItemQuality::LEGEND)
			kinahcount = 50000;
		else if (qualityOf(*resultItem.getItemTemplate()) == model::templates::item::ItemQuality::UNIQUE)
			kinahcount = 100000;

		if (!player.getInventory().tryDecreaseKinah(trade::PricesService::getPriceForService(kinahcount, player.getRace()))) {
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_STIGMA_NOT_ENOUGH_MONEY());
			return false;
		}
		addStigmaSkills(player, stigmaInfo, stigmaLevel);
	}
	return true;
}

void StigmaService::onPlayerLogin(model::gameobjects::player::Player& player) {
	if (player.hasPermission(configs::main::MembershipConfig::STIGMA_AUTOLEARN.load())) {
		for (int32_t level = 20; level <= player.getLevel(); level++) {
			for (const skillengine::model::SkillLearnTemplate* skillTemplate :
				dataholders::DataManager::SKILL_TREE_DATA->getTemplatesFor(player.getPlayerClass(), level, player.getRace())) {
				if (skillTemplate->isStigma())
					SkillLearnService::learnTemporarySkill(player, skillTemplate->getSkillId(), skillTemplate->getSkillLevel());
			}
		}
		return;
	}

	std::vector<runtime::Ptr<model::gameobjects::Item>> equippedStigmas = player.getEquipment().getEquippedItemsAllStigma();
	for (const runtime::Ptr<model::gameobjects::Item>& item : equippedStigmas) { // Java: mainLoop
		if (!item->getItemTemplate()->isStigma()) {
			player.getEquipment().unEquipItem(item->getObjectId(), false);
			log.warn("Unequipped stigma: " + std::to_string(item->getItemId()) + ", stigma info missing for item (possibly pre-4.8 stigma)");
			continue;
		}

		if (!isPossibleEquippedStigma(player, *item)) {
			player.getEquipment().unEquipItem(item->getObjectId(), false);
			utils::audit::AuditLogger::log(player, "had more equipped stigmas on login than allowed");
			continue;
		}

		if (!item->getItemTemplate()->isClassSpecific(player.getPlayerClass())) {
			player.getEquipment().unEquipItem(item->getObjectId(), false);
			utils::audit::AuditLogger::log(player, "had an equipped stigma on login which was not for his class");
			continue;
		}

		// check for double stigmas equipped into the same slot
		bool doubleStigma = false;
		for (const runtime::Ptr<model::gameobjects::Item>& checkStigma : player.getEquipment().getEquippedItemsAllStigma()) {
			if (checkStigma->getEquipmentSlot() == item->getEquipmentSlot() && checkStigma->getItemId() != item->getItemId()) {
				player.getEquipment().unEquipItem(item->getObjectId(), false);
				utils::audit::AuditLogger::log(player, "had two stigmas equipped in the same slot on login");
				doubleStigma = true;
				break;
			}
		}
		if (doubleStigma)
			continue; // Java: continue mainLoop

		addStigmaSkills(player, item->getItemTemplate()->getStigma(), item->getEnchantLevel());
	}

	addLinkedStigmaSkills(player);
}

// Java StigmaService.java:173-203
void StigmaService::removeLinkedStigmaSkills(model::gameobjects::player::Player& player) {
	std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>> linkedStigmaSkills;
	while (true) { // remove all linked stigma skills (can be more than one if stigma auto learning is enabled)
		std::optional<std::string> stack;
		linkedStigmaSkills.clear();
		for (const runtime::Ptr<model::skill::PlayerSkillEntry>& skill : player.getSkillList()->getAllSkills()) {
			if (skill->isLinkedStigmaSkill()) {
				if (!stack)
					stack = templateOf(*skill).getStack();
				if (equalsIgnoreCase(templateOf(*skill).getStack(), *stack))
					linkedStigmaSkills.push_back(skill);
				if (equalsIgnoreCase(*stack, "NONE"))
					break;
			}
		}
		if (linkedStigmaSkills.empty())
			break;

		// Java's null Strings: SM_SYSTEM_MESSAGE writes a null parameter as an empty string (writeS(null))
		std::string firstSkillL10n, secondSkillL10n;
		int32_t skillLevel = 0;
		for (size_t i = 0; i < linkedStigmaSkills.size(); i++) {
			const runtime::Ptr<model::skill::PlayerSkillEntry>& skillEntry = linkedStigmaSkills[i];
			SkillLearnService::removeSkill(player, skillEntry->getSkillId());
			if (i == 0) {
				firstSkillL10n = templateOf(*skillEntry).getL10n();
				skillLevel = skillEntry->getSkillLevel();
			} else if (i == 1)
				secondSkillL10n = templateOf(*skillEntry).getL10n();
		}
		utils::PacketSendUtility::sendPacket(player,
			network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_STIGMA_DELETE_HIDDEN_SKILL(firstSkillL10n, skillLevel, secondSkillL10n));
	}
}

void StigmaService::addLinkedStigmaSkills(model::gameobjects::player::Player& player) {
	std::vector<runtime::Ptr<model::gameobjects::Item>> stigmas = player.getEquipment().getEquippedItemsAllStigma();
	if (stigmas.size() < 6)
		return;

	for (const runtime::Ptr<model::gameobjects::Item>& stigma : stigmas) {
		if (!stigma->isStigmaChargeable())
			return;
	}

	int32_t skillId = getLinkedStigmaLearnSkill(player);
	if (skillId > 0) {
		// linked stigma level is the lowest enchant level of all equipped stigmas
		int32_t linkedStigmaSkillLevel = (*std::ranges::min_element(stigmas, {}, [](const runtime::Ptr<model::gameobjects::Item>& i) {
			return i->getEnchantLevel();
		}))->getEnchantLevel() + 1;
		for (const skillengine::model::SkillLearnTemplate* skill :
			dataholders::DataManager::SKILL_TREE_DATA->getSkillsForSkill(skillId, player.getPlayerClass(), player.getRace(), player.getLevel()))
			SkillLearnService::learnTemporarySkill(player, skill->getSkillId(), linkedStigmaSkillLevel);
	}
}

// Java StigmaService.java:226-305; references: http://aion.mouseclic.com/beta/stigma.php
int32_t StigmaService::getLinkedStigmaLearnSkill(model::gameobjects::player::Player& player) {
	using model::PlayerClass;
	bool isEly = player.getRace() == model::Race::ELYOS;
	switch (player.getPlayerClass()) {
		case PlayerClass::GLADIATOR:
			if (isEquipped(player, 140001118) && isEquipped(player, 2, {140001103, 140001104, 140001105}))
				return 731; // Wind Lance
			if (isEquipped(player, 140001119) && isEquipped(player, 2, {140001106, 140001107, 140001108}))
				return 643; // Unraveling Assault
			return !isEly ? 661 : 662; // Battle Banner
		case PlayerClass::TEMPLAR:
			if (isEquipped(player, 140001134) && isEquipped(player, 2, {140001120, 140001122, 140001125}))
				return 2921; // Invigorating Strike
			if (isEquipped(player, 140001135) && isEquipped(player, 2, {140001121, 140001123, 140001124}))
				return 2918; // Shield of Vengeance
			return 2917; // Eternal Denial
		case PlayerClass::ASSASSIN:
			if (isEquipped(player, 140001151) && isEquipped(player, 2, {140001136, 140001137, 140001140}))
				return 3241; // Fangdrop Stab
			if (isEquipped(player, 140001152) && isEquipped(player, 2, {140001138, 140001139, 140001141}))
				return 3238; // Shimmerbomb
			return 3244; // Explosive Rebranding
		case PlayerClass::RANGER:
			if (isEquipped(player, 140001172) && isEquipped(player, 2, {140001153, 140001155, 140001157}))
				return 1008; // Ripthread Shot
			if (isEquipped(player, 140001173) && isEquipped(player, 2, {140001154, 140001156, 140001158}))
				return 938; // Night Haze
			return isEly ? 1065 : 1064; // Staggering Trap
		case PlayerClass::SORCERER:
			if (isEquipped(player, 140001191) && isEquipped(player, 2, {140001174, 140001178, 140001181}))
				return 1342; // Slumberswept Wind
			if (isEquipped(player, 140001192) && isEquipped(player, 2, {140001176, 140001177, isEly ? 140001184 : 140001185}))
				return 1542; // Aetherblaze
			return 1420; // Repulsion Field
		case PlayerClass::SPIRIT_MASTER:
			if (isEquipped(player, 140001209) && isEquipped(player, 2, {140001193, 140001194, 140001195}))
				return 3543; // Spirit's Empowerment
			if (isEquipped(player, 140001210) && isEquipped(player, 2, {140001196, isEly ? 140001197 : 140001198, 140001199}))
				return 3549; // Command: Absorb Wounds
			return 3851; // Blood Funnel
		case PlayerClass::CLERIC:
			if (isEquipped(player, 140001245) && isEquipped(player, 2, {140001228, 140001229, isEly ? 140001230 : 140001231}))
				return 4169; // Judge's Edict
			if (isEquipped(player, 140001246) && isEquipped(player, 2, {140001232, 140001233, isEly ? 140001234 : 140001235}))
				return 3934; // Restoration Relief
			return isEly ? 3906 : 3911; // Summon Vexing Energy
		case PlayerClass::CHANTER:
			if (isEquipped(player, 140001226) && isEquipped(player, 2, {140001211, 140001212, 140001213}))
				return 1909; // Word of Instigation
			if (isEquipped(player, 140001227) && isEquipped(player, 2, {140001214, 140001215, 140001216}))
				return 1903; // Resonant Strike
			return 1906; // Debilitating Incantation
		case PlayerClass::RIDER:
			if (isEquipped(player, 140001279) && isEquipped(player, 2, {140001264, 140001265, 140001269}))
				return 2858; // Explosive Exhaust
			if (isEquipped(player, 140001280) && isEquipped(player, 2, {140001266, 140001267, 140001268}))
				return 2863; // Powerspike Trigger
			return 2851; // Nerve Pulse
		case PlayerClass::GUNNER:
			if (isEquipped(player, 140001262) && isEquipped(player, 2, {140001247, 140001248, 140001249}))
				return 2370; // Pursuit Stance
			if (isEquipped(player, 140001263) && isEquipped(player, 2, {140001250, 140001251, 140001252}))
				return 2377; // Sequential Fire
			return 2382; // Pulverizer Cannon
		case PlayerClass::BARD:
			if (isEquipped(player, 140001296) && isEquipped(player, 2, {140001281, 140001282, 140001284}))
				return 4480; // Blazing Requiem
			if (isEquipped(player, 140001297) && isEquipped(player, 2, {140001283, 140001285, 140001286}))
				return 4483; // Purging Paean
			return 4566; // Delusional Dirge
		default:
			break;
	}
	return 0;
}

// Java StigmaService.java:307-311 (Equipment.getEquippedItemsByItemId never answers null: the C++ list is the same check)
bool StigmaService::isEquipped(model::gameobjects::player::Player& player, int32_t itemId) {
	return !player.getEquipment().getEquippedItemsByItemId(itemId).empty();
}

// Java StigmaService.java:313-319
bool StigmaService::isEquipped(model::gameobjects::player::Player& player, int32_t neededCount, std::initializer_list<int32_t> itemIds) {
	int32_t equippedCount = 0;
	for (int32_t itemId : itemIds)
		if (isEquipped(player, itemId))
			equippedCount += 1;
	return equippedCount == neededCount;
}

// Java StigmaService.java:321-337
int32_t StigmaService::getPossibleStigmaCount(model::gameobjects::player::Player& player) {
	if (player.hasPermission(configs::main::MembershipConfig::STIGMA_SLOT_QUEST.load()))
		return 3;

	int32_t playerLevel = player.getLevel();
	bool isCompleteQuest = StigmaService::isCompleteQuest(player);

	if (isCompleteQuest) {
		if (playerLevel < 30)
			return 1;
		else if (playerLevel < 40)
			return 2;
		else
			return 3;
	}
	return 0;
}

// Java StigmaService.java:339-358 - Stigma Quest Elyos: 1929, Asmodians: 2900
bool StigmaService::isCompleteQuest(model::gameobjects::player::Player& player) {
	using questEngine::model::QuestStatus;
	bool isCompleteQuest = false;

	if (player.getRace() == model::Race::ELYOS) {
		runtime::Ptr<questEngine::model::QuestState> qs = player.getQuestStateList()->getQuestState(1929);
		if (qs)
			isCompleteQuest = player.isCompleteQuest(1929) || (qs->getStatus() == QuestStatus::START && qs->getQuestVars()->getQuestVars() == 98);
		else
			isCompleteQuest = player.isCompleteQuest(1929);
	} else {
		runtime::Ptr<questEngine::model::QuestState> qs = player.getQuestStateList()->getQuestState(2900);
		if (qs)
			isCompleteQuest = player.isCompleteQuest(2900) || (qs->getStatus() == QuestStatus::START && qs->getQuestVars()->getQuestVars() == 99);
		else
			isCompleteQuest = player.isCompleteQuest(2900);
	}
	return isCompleteQuest;
}

// Java StigmaService.java:360-376
int32_t StigmaService::getPossibleAdvancedStigmaCount(model::gameobjects::player::Player& player) {
	if (player.hasPermission(configs::main::MembershipConfig::STIGMA_SLOT_QUEST.load()))
		return 3;
	int32_t playerLevel = player.getLevel();
	bool isCompleteQuest = StigmaService::isCompleteQuest(player);
	if (isCompleteQuest) {
		if (playerLevel >= 55)
			return 3;
		else if (playerLevel >= 50)
			return 2;
		else if (playerLevel >= 45)
			return 1;
	}
	return 0;
}

// Java StigmaService.java:378-414
bool StigmaService::isPossibleEquippedStigma(model::gameobjects::player::Player& player, model::gameobjects::Item& item) {
	if (!item.getItemTemplate()->isStigma())
		return false;

	int64_t itemSlotToEquip = item.getEquipmentSlot();

	// Stigma
	if (model::items::isRegularStigma(itemSlotToEquip)) {
		int32_t stigmaCount = getPossibleStigmaCount(player);
		if (stigmaCount > 0) {
			if (stigmaCount == 1) {
				if (itemSlotToEquip == model::items::getSlotIdMask(model::items::ItemSlot::STIGMA1))
					return true;
			} else if (stigmaCount == 2) {
				if (itemSlotToEquip == model::items::getSlotIdMask(model::items::ItemSlot::STIGMA1) || itemSlotToEquip == model::items::getSlotIdMask(model::items::ItemSlot::STIGMA2))
					return true;
			} else if (stigmaCount == 3)
				return true;
		}
	}
	// Advanced Stigma
	else if (model::items::isAdvancedStigma(itemSlotToEquip)) {
		int32_t advStigmaCount = getPossibleAdvancedStigmaCount(player);
		if (advStigmaCount > 0) {
			if (advStigmaCount == 1) {
				if (itemSlotToEquip == model::items::getSlotIdMask(model::items::ItemSlot::ADV_STIGMA1))
					return true;
			} else if (advStigmaCount == 2) {
				if (itemSlotToEquip == model::items::getSlotIdMask(model::items::ItemSlot::ADV_STIGMA1) || itemSlotToEquip == model::items::getSlotIdMask(model::items::ItemSlot::ADV_STIGMA2))
					return true;
			} else if (advStigmaCount == 3)
				return true;
		}
	}
	return false;
}

// Java StigmaService.java:416-460. The anonymous ItemUseObserver (StigmaService$1) is the struct above; the anonymous Runnable
// (StigmaService$2) is a lambda pinned to the objects it captures (the player, the observer, the stigma and the charge stone) and stored as the
// controller's ITEM_USE task as Java stores its Future
void StigmaService::chargeStigma(model::gameobjects::player::Player& player, model::gameobjects::Item& stigma,
	model::gameobjects::Item& chargeStone) {
	using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	// Java reads `Stigma stigmaInfo = stigma.getItemTemplate().getStigma()` here; the task below reads it (no side effect moves)
	if (stigma.getItemId() != chargeStone.getItemId() || chargeStone.getEnchantLevel() > 0 || stigma.getEnchantLevel() >= 10)
		return;
	if (!stigma.isStigmaChargeable())
		return;

	const bool isSuccess = commons::utils::Rnd::chance() < std::max(25, 100 - (stigma.getEnchantLevel() * 10));

	const int32_t parentItemId = stigma.getItemId();
	const int32_t parentObjectId = stigma.getObjectId();
	utils::PacketSendUtility::broadcastPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentObjectId, chargeStone.getObjectId(), parentItemId, 5000, 0, 0), true);
	runtime::Ref<StigmaService_ItemUseObserver> observer = StigmaService_ItemUseObserver::create(player, chargeStone, parentItemId, parentObjectId);
	player.getObserveController()->attach(*observer);
	StigmaService_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(model::TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({&player, &itemUseObserver, &stigma, &chargeStone},
			[&player, &itemUseObserver, &stigma, &chargeStone, isSuccess, parentItemId, parentObjectId] {
				// Java captures the local stigmaInfo; it is the stigma's immutable item template data, read again from the pinned stigma
				const model::templates::item::Stigma* stigmaInfo = stigma.getItemTemplate()->getStigma();
				player.getObserveController()->removeObserver(itemUseObserver);
				utils::PacketSendUtility::broadcastPacket(player,
					SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentObjectId, parentItemId, 0, isSuccess ? 1 : 2, 1), true);
				if (!player.getInventory().decreaseByObjectId(chargeStone.getObjectId(), 1, item::ItemPacketService_ItemUpdateType::DEC_STIGMA_USE))
					return;
				if (!isSuccess) {
					if (stigma.isEquipped())
						player.getEquipment().unEquipItem(stigma.getObjectId());
					player.getInventory().decreaseByObjectId(stigma.getObjectId(), 1, item::ItemPacketService_ItemUpdateType::DEC_STIGMA_USE);
					utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_STIGMA_ENCHANT_FAIL(stigma.getL10n()));
				} else {
					stigma.setEnchantLevel(stigma.getEnchantLevel() + 1);
					if (stigma.isEquipped()) {
						removeStigmaSkills(player, stigmaInfo, stigma.getEnchantLevel() - 1, false);
						addStigmaSkills(player, stigmaInfo, stigma.getEnchantLevel());
					}
					utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_STIGMA_ENCHANT_SUCCESS(stigma.getL10n()));
					utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM(player, stigma));
					if (stigma.getPersistentState() != model::gameobjects::Persistable_PersistentState::DELETED) {
						stigma.setPersistentState(model::gameobjects::Persistable_PersistentState::UPDATE_REQUIRED);
						if (stigma.isEquipped())
							player.getEquipment().setPersistentState(model::gameobjects::Persistable_PersistentState::UPDATE_REQUIRED);
						else
							player.getInventory().setPersistentState(model::gameobjects::Persistable_PersistentState::UPDATE_REQUIRED);
					}
				}
			},
			5000));
}

// Java StigmaService.java:462-468
void StigmaService::addStigmaSkills(model::gameobjects::player::Player& player, const model::templates::item::Stigma* stigma, int32_t stigmaLevel) {
	if (stigma == nullptr) // Java: stigma.getGainSkillGroups() on null
		throw runtime::NullPointerException("stigma");
	for (const std::string& skillGroup : stigma->getGainSkillGroups())
		for (const skillengine::model::SkillTemplate* st : skillTemplatesByGroup(skillGroup))
			for (const skillengine::model::SkillLearnTemplate* skill :
				dataholders::DataManager::SKILL_TREE_DATA->getTemplatesForSkill(st->getSkillId(), player.getPlayerClass(), player.getRace()))
				if (player.getLevel() >= skill->getMinLevel())
					SkillLearnService::learnTemporarySkill(player, skill->getSkillId(), stigmaLevel + 1);
}

// Java StigmaService.java:470-484 (stigmaLevel is unused in Java as well)
void StigmaService::removeStigmaSkills(model::gameobjects::player::Player& player, const model::templates::item::Stigma* stigma, int32_t /*stigmaLevel*/,
	bool notifyPlayer) {
	if (stigma == nullptr) // Java: stigma.getGainSkillGroups() on null
		throw runtime::NullPointerException("stigma");
	std::vector<std::string> notifiedSkillL10ns;
	for (const std::string& skillGroup : stigma->getGainSkillGroups()) {
		for (const skillengine::model::SkillTemplate* st : skillTemplatesByGroup(skillGroup)) {
			// Java `st.getL10n() != null`: the C++ L10n answers a string; an absent name is the empty one
			const std::string l10n = st->getL10n();
			if (notifyPlayer && !l10n.empty() && std::ranges::find(notifiedSkillL10ns, l10n) == notifiedSkillL10ns.end()) {
				notifiedSkillL10ns.push_back(l10n);
				utils::PacketSendUtility::sendPacket(player,
					network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_STIGMA_YOU_CANNOT_USE_THIS_SKILL_AFTER_UNEQUIP_STIGMA_STONE(l10n));
			}
			for (const skillengine::model::SkillLearnTemplate* skill :
				dataholders::DataManager::SKILL_TREE_DATA->getTemplatesForSkill(st->getSkillId(), player.getPlayerClass(), player.getRace()))
				SkillLearnService::removeSkill(player, skill->getSkillId());
		}
	}
	removeLinkedStigmaSkills(player);
}

} // namespace aion::gameserver::services
