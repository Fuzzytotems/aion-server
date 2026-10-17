#include "aion/gameserver/handlers/ai/events/iceFestival/IceSculptureAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::events::iceFestival {

AION_AI(IceSculptureAI, "icefestival_ice_sculpture");

// Java IceSculptureAI.java:18-25
void IceSculptureAI::handleUseItemFinish(Player& player) {
	ActionItemNpcAI::handleUseItemFinish(player);
	if (commons::utils::Rnd::chance() < 66)
		SkillEngine::getInstance().getSkill(getOwner(), 11020, 60, runtime::Ptr<VisibleObject>(player))->useSkill(); // world_event_icebuff_stat
	else
		SkillEngine::getInstance().getSkill(getOwner(), 22718, 60, runtime::Ptr<VisibleObject>(player))->useWithoutPropSkill(); // world_event_npc_frozen
}

} // namespace aion::gameserver::handlers::ai::events::iceFestival
