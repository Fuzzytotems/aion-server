#include "aion/gameserver/handlers/ai/events/BirthdayCakeAI.h"

#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::events {

AION_AI(BirthdayCakeAI, "birthday_cake");

// Java BirthdayCakeAI.java:22-31
bool BirthdayCakeAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	switch (dialogActionId) {
		case model::DialogAction::SETPRO1:
			SkillEngine::getInstance().getSkill(getOwner(), 10821, 1, runtime::Ptr<VisibleObject>(player))->useWithoutPropSkill();
			SkillEngine::getInstance().getSkill(getOwner(), 10822, 1, runtime::Ptr<VisibleObject>(player))->useWithoutPropSkill();
			return true;
	}
	return false;
}

} // namespace aion::gameserver::handlers::ai::events
