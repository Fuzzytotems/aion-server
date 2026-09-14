#include "aion/gameserver/services/autogroup/AutoGroupUtility.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::autogroup {

bool AutoGroupUtility::canRegisterNewEntry(model::gameobjects::player::Player& player, model::autogroup::AutoGroupType agt) {
	AION_UNPORTED();
}

bool AutoGroupUtility::canRegisterQuickEntry(model::gameobjects::player::Player& player, model::autogroup::AutoGroupType agt) {
	AION_UNPORTED();
}

bool AutoGroupUtility::canRegisterGroupEntry(model::gameobjects::player::Player& player, model::autogroup::AutoGroupType agt, int32_t mapId, int32_t maskId) {
	AION_UNPORTED();
}

bool AutoGroupUtility::checkGroupRequirements(model::gameobjects::player::Player& player, model::autogroup::AutoGroupType agt, int32_t mapId, int32_t maskId) {
	AION_UNPORTED();
}

void AutoGroupUtility::sendSuccessfulRegistration(model::autogroup::LookingForParty& lfp, std::string_view leaderName, model::autogroup::AutoGroupType agt, int32_t maskId) {
	AION_UNPORTED();
}

void AutoGroupUtility::sendWindowToPlayerIfOnline(int32_t objectId, int32_t maskId, int32_t windowId) {
	AION_UNPORTED();
}

void AutoGroupUtility::sendWindowToPlayer(model::gameobjects::player::Player& player, int32_t maskId, int32_t windowId) {
	AION_UNPORTED();
}

bool AutoGroupUtility::hasCoolDown(model::gameobjects::player::Player& player, int32_t worldId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::autogroup
