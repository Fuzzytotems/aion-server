#include "aion/gameserver/handlers/ai/portals/PortalAI.h"

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/Portal2Data.h"
#include "aion/gameserver/dataholders/TeleporterData.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/services/teleport/PortalService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"

namespace aion::gameserver::handlers::ai::portals {

AION_AI(PortalAI, "portal");

// Java PortalAI.java:30-33
bool PortalAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	return true;
}

// Java PortalAI.java:35-39
void PortalAI::handleSpawned() {
	ActionItemNpcAI::handleSpawned();
	teleportTemplate.set(DataManager::TELEPORTER_DATA->getTeleporterTemplateByNpcId(getNpcId()));
}

// Java PortalAI.java:41-45
void PortalAI::handleDialogStart(Player& player) {
	QuestEngine::getInstance().onDialog(*QuestEnv::create(runtime::Ptr<VisibleObject>(getOwner()), player, 0, model::DialogAction::USE_OBJECT));
	ActionItemNpcAI::handleDialogStart(player);
}

// Java PortalAI.java:47-57
void PortalAI::handleUseItemFinish(Player& player) {
	const PortalPath* portalPath = DataManager::PORTAL2_DATA->getPortalUsePath(getNpcId(), player);
	if (portalPath != nullptr) {
		PortalService::port(portalPath, player, getOwner());
	} else if (teleportTemplate.get() != nullptr) {
		TeleportService::teleportToFirstTeleportLocation(player, getOwner(), TeleportAnimation::FADE_OUT_BEAM);
	} else {
		ActionItemNpcAI::handleUseItemFinish(player);
	}
}

} // namespace aion::gameserver::handlers::ai::portals
