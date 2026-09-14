#include "aion/gameserver/services/StigmaService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

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
	AION_UNPORTED();
}

void StigmaService::removeLinkedStigmaSkills(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void StigmaService::addLinkedStigmaSkills(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
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
