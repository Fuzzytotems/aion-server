#include "aion/gameserver/handlers/ai/instance/drakenspire/Lv3HumanBeritraAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(Lv3HumanBeritraAI, "drakenspire_lv3_human_beritra");

// Java Lv3HumanBeritraAI.java:38-41
void Lv3HumanBeritraAI::handleLastSealBlasted() {
	getOwner().queueSkill(20842, 56);
}

// Java Lv3HumanBeritraAI.java:46-56
void Lv3HumanBeritraAI::handleThirdPhaseStarted() {
	if (getEffectController()->findBySkillId(21611) != nullptr)
		SkillEngine::getInstance().applyEffectDirectly(21611, getOwner(), getOwner());
	if (getEffectController()->findBySkillId(21610) != nullptr)
		SkillEngine::getInstance().applyEffectDirectly(21610, getOwner(), getOwner());
	PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_IDSEAL_VRITRA_HUMAN_03());
	getLifeStats()->setCurrentHp(getLifeStats()->getMaxHp());
	getOwner().getGameStats()->setFightStartingTime();
	resetBlastedSeal();
}

// Java Lv3HumanBeritraAI.java:58-62
void Lv3HumanBeritraAI::handleAttack(runtime::Ptr<Creature> creature) {
	hpPhases->tryEnterNextPhase(*this);
	Lv2HumanBeritraAI::handleAttack(creature);
}

// Java Lv3HumanBeritraAI.java:64-71
void Lv3HumanBeritraAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 20842) {
		// Retail would apply this through activate_skillarea
		getPosition()->getWorldMapInstance()->forEachPlayer([this](Player& p) { SkillEngine::getInstance().applyEffectDirectly(20842, getOwner(), p); });
		handleTransformation();
	}
}

// Java Lv3HumanBeritraAI.java:73-77
void Lv3HumanBeritraAI::handleTransformation() {
	for (const runtime::Ptr<Npc>& npc : getPosition()->getWorldMapInstance()->getNpcs({702695, 702696}))
		npc->getController().die(); // Collapse ceiling and ambient light
	spawn(236247, 123.055f, 519.190f, 1750.3414f, int8_t{0}); // Dragon Form
	AIActions::deleteOwner(*this);
}

// Java Lv3HumanBeritraAI.java:79-82
void Lv3HumanBeritraAI::handleHpPhase(int32_t phaseHpPercent) {
	handleLastSealBlasted(); // Using the same logic here
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
