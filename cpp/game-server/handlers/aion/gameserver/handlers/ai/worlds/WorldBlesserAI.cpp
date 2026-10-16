#include "aion/gameserver/handlers/ai/worlds/WorldBlesserAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::worlds {

AION_AI(WorldBlesserAI, "world_blesser");

// Java WorldBlesserAI.java:24-40
void WorldBlesserAI::handleDialogStart(Player& player) {
	switch (getNpcId()) {
		case 831030: // Netalion
		case 831024: // Renniah
		case 831025: // Erdat
		case 831026: // Erdat
		case 831028: // Erdat
		case 831027: // Karzanke
			GeneralNpcAI::handleDialogStart(player);
			break;
		default: {
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 1011));
			break;
		}
	}
}

// Java WorldBlesserAI.java:42-57
bool WorldBlesserAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	runtime::Ref<QuestEnv> env = QuestEnv::create(runtime::Ptr<VisibleObject>(getOwner()), player, questId, dialogActionId);
	env->setExtendedRewardIndex(extendedRewardIndex);
	if (QuestEngine::getInstance().onDialog(*env) && dialogActionId != model::DialogAction::SETPRO1) {
		return true;
	}
	if (dialogActionId == model::DialogAction::SETPRO1) {
		// int chance = Rnd.get(1, 2);
		// 951: Blessing of Health I, 955: Blessing of Rock I : 3.9
		// 20950 : Blessing of Growth : 4.0
		runtime::Ref<skillengine::model::Skill> skill = SkillEngine::getInstance().getSkill(getOwner(), 20950, 1, runtime::Ptr<VisibleObject>(player));
		if (skill == nullptr) // Java: getSkill(...).useWithoutPropSkill() on null
			throw runtime::NullPointerException("WorldBlesserAI: skill 20950");
		skill->useWithoutPropSkill();
	} else if (dialogActionId == model::DialogAction::QUEST_SELECT && questId != 0) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), 10, questId));
	}
	return true;
}

} // namespace aion::gameserver::handlers::ai::worlds
