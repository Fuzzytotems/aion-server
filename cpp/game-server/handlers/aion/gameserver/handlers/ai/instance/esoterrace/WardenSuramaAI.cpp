#include "aion/gameserver/handlers/ai/instance/esoterrace/WardenSuramaAI.h"

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::esoterrace {

AION_AI(WardenSuramaAI, "wardensurama");

// Java WardenSuramaAI.java:27-31
void WardenSuramaAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java WardenSuramaAI.java:33-35
void WardenSuramaAI::handleHpPhase(int32_t /*phaseHpPercent*/) {
	spawnGeysers();
}

// Java WardenSuramaAI.java:37-51
void WardenSuramaAI::spawnGeysers() {
	skillengine::SkillEngine::getInstance().getSkill(getOwner(), 19332, 50, runtime::Ptr<VisibleObject>(getOwner()))->useNoAnimationSkill();
	spawn(282171, 1317.097656f, 1145.419556f, 53.203529f, int8_t{0}, 595);
	spawn(282172, 1343.426147f, 1170.675293f, 53.203529f, int8_t{0}, 596);
	spawn(282173, 1316.953979f, 1196.861328f, 53.203529f, int8_t{0}, 598);
	spawn(282174, 1290.778442f, 1170.730957f, 53.203529f, int8_t{0}, 597);
	spawn(282425, 1305.310059f, 1159.337769f, 53.203529f, int8_t{0}, 721);
	spawn(282426, 1328.446289f, 1159.062500f, 53.203529f, int8_t{0}, 718);
	spawn(282427, 1328.613770f, 1182.369873f, 53.203529f, int8_t{0}, 722);
	spawn(282428, 1305.083130f, 1182.424927f, 53.203529f, int8_t{0}, 719);
	PacketSendUtility::broadcastPacket(getOwner(), network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_IDF4Re_Drana_10());
	PacketSendUtility::broadcastPacket(getOwner(), network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_IDF4Re_Drana_09());
	ThreadPoolManager::getInstance().schedule({this}, [this] { despawnGeysers(); }, 13000);
}

// Java WardenSuramaAI.java:53-56
void WardenSuramaAI::despawnGeysers() {
	for (const runtime::Ptr<Npc>& npc :
		getPosition()->getWorldMapInstance()->getNpcs({282171, 282172, 282173, 282174, 282425, 282426, 282427, 282428}))
		npc->getController().delete_();
}

// Java WardenSuramaAI.java:58-61
void WardenSuramaAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
}

} // namespace aion::gameserver::handlers::ai::instance::esoterrace
