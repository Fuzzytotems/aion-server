#include "aion/gameserver/services/StigmaService.h"

#include <algorithm>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/services/SkillLearnService.h"
#include "aion/gameserver/skillengine/model/SkillLearnTemplate.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::services {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
// anonymous ItemUseObserver at StigmaService.java:376 (com.aionemu.gameserver.services.StigmaService$1); local observer; storage: stored in
// ObserveController
//   anonymous Runnable at StigmaService.java:388 (com.aionemu.gameserver.services.StigmaService$2); argument 1 of schedule(); storage: task

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.StigmaService");

bool StigmaService::notifyEquipAction(model::gameobjects::player::Player& player, model::gameobjects::Item& resultItem, int64_t slot) {
	AION_UNPORTED();
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
