#include "aion/gameserver/handlers/ai/instance/theobomosLab/SilikorofMemoryAI.h"

#include <cmath>
#include <numbers>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::theobomosLab {

AION_AI(SilikorofMemoryAI, "silikor");

// Java SilikorofMemoryAI.java:25-29
void SilikorofMemoryAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java SilikorofMemoryAI.java:31-35
void SilikorofMemoryAI::handleHpPhase(int32_t /*phaseHpPercent*/) {
	sp(281054);
	sp(281053);
}

// Java SilikorofMemoryAI.java:37-44
void SilikorofMemoryAI::sp(int32_t npcId) {
	double angleRadians = static_cast<double>(commons::utils::Rnd::nextFloat(360.0f)) / 180.0 * std::numbers::pi; // Java: Math.toRadians
	int32_t distance = commons::utils::Rnd::get(0, 2);
	float x1 = static_cast<float>(std::cos(angleRadians) * distance);
	float y1 = static_cast<float>(std::sin(angleRadians) * distance);
	runtime::Ptr<WorldPosition> p = getPosition();
	spawn(npcId, p->getX() + x1, p->getY() + y1, p->getZ(), p->getHeading());
}

// Java SilikorofMemoryAI.java:46-51
void SilikorofMemoryAI::handleSpawned() {
	AggressiveNpcAI::handleSpawned();
	if (getNpcId() == 214668)
		SkillEngine::getInstance().getSkill(getOwner(), 18481, 1, runtime::Ptr<VisibleObject>(getOwner()))->useSkill();
}

// Java SilikorofMemoryAI.java:53-57
void SilikorofMemoryAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
}

// Java SilikorofMemoryAI.java:59-63
void SilikorofMemoryAI::handleDied() {
	for (const runtime::Ptr<Npc>& npc : getPosition()->getWorldMapInstance()->getNpcs({281054, 281053}))
		npc->getController().delete_();
	AggressiveNpcAI::handleDied();
}

} // namespace aion::gameserver::handlers::ai::instance::theobomosLab
