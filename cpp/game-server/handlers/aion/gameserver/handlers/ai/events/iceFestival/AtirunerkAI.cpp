#include "aion/gameserver/handlers/ai/events/iceFestival/AtirunerkAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"

namespace aion::gameserver::handlers::ai::events::iceFestival {

AION_AI(AtirunerkAI, "atirunerk");

// Java AtirunerkAI.java:19-23
void AtirunerkAI::handleDialogStart(Player& player) {
	GeneralNpcAI::handleDialogStart(player);
	QuestEngine::getInstance().onDialog(*QuestEnv::create(runtime::Ptr<VisibleObject>(getOwner()), player, 80719, model::DialogAction::QUEST_SELECT));
}

} // namespace aion::gameserver::handlers::ai::events::iceFestival
