#include "aion/gameserver/handlers/ai/instance/sauroBase/GuardCaptainAhuradimGenerator.h"

#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::handlers::ai::instance::sauroBase {

AION_AI(GuardCaptainAhuradimGenerator, "guard_captain_ahuradim_generator");

// Java GuardCaptainAhuradimGenerator.java:18-25
void GuardCaptainAhuradimGenerator::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21200) {
		runtime::Ptr<VisibleObject> guardCaptainAhuradim = getKnownList().findObject([](world::knownlist::KnownObject& o) {
			runtime::Ptr<Npc> npc = runtime::as<Npc>(o.get());
			return npc != nullptr && npc->getNpcId() == 230857;
		});
		if (guardCaptainAhuradim != nullptr)
			SkillEngine::getInstance().getSkill(getOwner(), 21191, 1, guardCaptainAhuradim)->useSkill();
	}
}

} // namespace aion::gameserver::handlers::ai::instance::sauroBase
