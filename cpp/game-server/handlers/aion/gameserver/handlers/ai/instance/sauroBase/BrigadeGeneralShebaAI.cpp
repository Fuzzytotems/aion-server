#include "aion/gameserver/handlers/ai/instance/sauroBase/BrigadeGeneralShebaAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::handlers::ai::instance::sauroBase {

AION_AI(BrigadeGeneralShebaAI, "brigade_general_sheba");

// Java BrigadeGeneralShebaAI.java:25-28
float BrigadeGeneralShebaAI::modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) {
	return damage * multiplier.get();
}

// Java BrigadeGeneralShebaAI.java:30-56
void BrigadeGeneralShebaAI::onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	multiplier = 1.0f;
	switch (skillTemplate->getSkillId()) {
		case 21188:
			PacketSendUtility::broadcastMessage(getOwner(), 1500775);
			multiplier = 0.55f;
			break;
		case 21183:
			PacketSendUtility::broadcastMessage(getOwner(), 1500779);
			multiplier = 0.55f;
			break;
		case 21187:
		case 21410:
		case 21425:
		case 21450:
			multiplier = 0.55f;
			break;
		case 21186:
			PacketSendUtility::broadcastMessage(getOwner(), 1500780);
			multiplier = 0.55f;
			break;
		case 21189:
			PacketSendUtility::broadcastMessage(getOwner(), 1500774);
			break;
		default:
			break;
	}
}

// Java BrigadeGeneralShebaAI.java:58-88
void BrigadeGeneralShebaAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	multiplier = 1.0f;
	switch (skillTemplate->getSkillId()) {
		case 21189: // danuar henchman
			spawn(284435, 891.17f, 880.87f, 411.54f, int8_t{14});
			spawn(284435, 890.8f, 898.97f, 411.46f, int8_t{105});
			spawn(284435, 904.95f, 880.1f, 411.34f, int8_t{40});
			spawn(284435, 905.95f, 900.22f, 411.34f, int8_t{80});
			break;
		case 21184: // danuar channeling
			if (spawnCenter.get()) {
				spawn(284436, 900.1f, 889.84f, 411.54f, int8_t{20});
				spawnCenter = false;
			} else {
				spawn(284436, 887.63f, 902.31f, 411.46f, int8_t{105});
				spawn(284436, 882.2f, 889.77f, 411.46f, int8_t{120});
				spawn(284436, 887.59f, 877.32f, 411.46f, int8_t{15});
				spawn(284436, 900.0f, 872.28f, 411.46f, int8_t{30});
				spawn(284436, 912.39f, 877.44f, 411.46f, int8_t{45});
				spawn(284436, 917.2f, 889.89f, 411.46f, int8_t{60});
				spawn(284436, 912.33f, 902.0f, 411.46f, int8_t{75});
				spawn(284436, 900.0f, 907.16f, 411.46f, int8_t{90});
				spawnCenter = true;
			}
			break;
		case 21186:
			PacketSendUtility::broadcastMessage(getOwner(), 1500781);
			break;
		default:
			break;
	}
}

// Java BrigadeGeneralShebaAI.java:90-94
void BrigadeGeneralShebaAI::handleBackHome() {
	removeAdds();
	AggressiveNpcAI::handleBackHome();
}

// Java BrigadeGeneralShebaAI.java:96-100
void BrigadeGeneralShebaAI::handleDied() {
	removeAdds();
	AggressiveNpcAI::handleDied();
}

// Java BrigadeGeneralShebaAI.java:102-104
void BrigadeGeneralShebaAI::removeAdds() {
	for (const runtime::Ptr<Npc>& npc : getOwner().getWorldMapInstance()->getNpcs({284435, 284436}))
		npc->getController().delete_();
}

} // namespace aion::gameserver::handlers::ai::instance::sauroBase
