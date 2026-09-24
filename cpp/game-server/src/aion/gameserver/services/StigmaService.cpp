#include "aion/gameserver/services/StigmaService.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemQuality.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/SkillLearnService.h"
#include "aion/gameserver/services/trade/PricesService.h"
#include "aion/gameserver/skillengine/model/SkillLearnTemplate.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
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

void StigmaService::removeLinkedStigmaSkills(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
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

int32_t StigmaService::getLinkedStigmaLearnSkill(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool StigmaService::isEquipped(model::gameobjects::player::Player& player, int32_t itemId) {
	AION_UNPORTED();
}

bool StigmaService::isEquipped(model::gameobjects::player::Player& player, int32_t neededCount, std::initializer_list<int32_t> itemIds) {
	AION_UNPORTED();
}

int32_t StigmaService::getPossibleStigmaCount(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool StigmaService::isCompleteQuest(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

int32_t StigmaService::getPossibleAdvancedStigmaCount(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool StigmaService::isPossibleEquippedStigma(model::gameobjects::player::Player& player, model::gameobjects::Item& item) {
	AION_UNPORTED();
}

void StigmaService::chargeStigma(model::gameobjects::player::Player& player, model::gameobjects::Item& stigma,
	model::gameobjects::Item& chargeStone) {
	AION_UNPORTED();
}

void StigmaService::addStigmaSkills(model::gameobjects::player::Player& player, const model::templates::item::Stigma* stigma, int32_t stigmaLevel) {
	AION_UNPORTED();
}

void StigmaService::removeStigmaSkills(model::gameobjects::player::Player& player, const model::templates::item::Stigma* stigma, int32_t stigmaLevel,
	bool notifyPlayer) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
