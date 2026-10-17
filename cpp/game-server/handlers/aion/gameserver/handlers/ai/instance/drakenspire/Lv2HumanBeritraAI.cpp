#include "aion/gameserver/handlers/ai/instance/drakenspire/Lv2HumanBeritraAI.h"

#include <vector>

#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(Lv2HumanBeritraAI, "drakenspire_lv2_human_beritra");

// Java Lv2HumanBeritraAI.java:37-40
void Lv2HumanBeritraAI::handleFightStarted() {
	// Don't do anything here.
}

// Java Lv2HumanBeritraAI.java:42-45
void Lv2HumanBeritraAI::handleThirdPhaseStarted() {
	if (getEffectController()->findBySkillId(21612) != nullptr)
		spawnFactionHelpers(getAttackingPlayerRace() == Race::ELYOS ? std::vector<int32_t>{209734, 209735, 209735}
		                                                             : std::vector<int32_t>{209799, 209800, 209800});
}

// Java Lv2HumanBeritraAI.java:47-49
void Lv2HumanBeritraAI::handleLastSealBlasted() {
	// Don't do anything here.
}

// Java Lv2HumanBeritraAI.java:51-67
void Lv2HumanBeritraAI::onEffectApplied(Effect& effect) {
	if (effect.getSkillId() == 21624) { // Dragon Lord Seal
		switch (sealsBlasted.incrementAndGet()) {
			case 1:
				PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501272); // You insects think you have a chance against me?
				spawnPustules();
				break;
			case 2:
				PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501271); // Fregion! You left me to clean up your mess...
				spawnPustules();
				break;
			case 3:
				handleLastSealBlasted();
				break;
		}
	}
	Lv1HumanBeritraAI::onEffectApplied(effect);
}

// Java Lv2HumanBeritraAI.java:69-74
void Lv2HumanBeritraAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	if (skillTemplate->getSkillId() == 21609 && skillLevel == 58)
		handleThirdPhaseStarted();
	Lv1HumanBeritraAI::onEndUseSkill(skillTemplate, skillLevel);
}

// Java Lv2HumanBeritraAI.java:76-80
void Lv2HumanBeritraAI::spawnPustules() {
	spawn(855446, 135.192f, 502.221f, 1749.448f, int8_t{0}); // Drakenspire Pustule
	spawn(855446, 167.133f, 502.114f, 1749.448f, int8_t{30}); // Drakenspire Pustule
	spawn(855446, 161.501f, 535.842f, 1749.367f, int8_t{90}); // Drakenspire Pustule
}

// Java Lv2HumanBeritraAI.java:82-84
void Lv2HumanBeritraAI::resetBlastedSeal() {
	sealsBlasted.set(0);
}

// Java Lv2HumanBeritraAI.java:86-90
void Lv2HumanBeritraAI::handleBackHome() {
	Lv1HumanBeritraAI::handleBackHome();
	resetBlastedSeal();
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
