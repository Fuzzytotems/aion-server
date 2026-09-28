#include "aion/gameserver/services/ClassChangeService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

void ClassChangeService::showClassChangeDialog(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void ClassChangeService::changeClassToSelection(model::gameobjects::player::Player& player, int32_t dialogActionId) {
	AION_UNPORTED();
}

void ClassChangeService::completeAscensionQuest(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool ClassChangeService::setClass(model::gameobjects::player::Player& player, model::PlayerClass newClass) {
	AION_UNPORTED();
}

bool ClassChangeService::setClass(model::gameobjects::player::Player& player, std::optional<model::PlayerClass> newClass, bool validate, bool updateDaevaStatus) {
	AION_UNPORTED();
}

int32_t ClassChangeService::getClassSelectionDialogPageId(model::Race playerRace, model::PlayerClass playerClass) {
	AION_UNPORTED();
}

std::optional<model::PlayerClass> ClassChangeService::getSelectedPlayerClass(model::Race race, int32_t dialogActionId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
