#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/ExplosionShadowsAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

AION_AI(ExplosionShadowsAI, "explosion_shadows");

// Java ExplosionShadowsAI.java:27-31
void ExplosionShadowsAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	getPosition()->getWorldMapInstance()->setDoorState(2, false); // this actually opens it on client side (wtf)
}

// Java ExplosionShadowsAI.java:33-38
void ExplosionShadowsAI::handleDespawned() {
	AggressiveNpcAI::handleDespawned();
	getPosition()->getWorldMapInstance()->setDoorState(17, false); // this actually opens it on client side (wtf)
	getPosition()->getWorldMapInstance()->setDoorState(2, false); // this actually opens it on client side (wtf)
}

// Java ExplosionShadowsAI.java:40-45
void ExplosionShadowsAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	AggressiveNpcAI::onEndUseSkill(skillTemplate, skillLevel);
	if (skillTemplate->getSkillId() == 19425) // Self Destruct
		ThreadPoolManager::getInstance().schedule({this}, [this] { check(); }, 1500);
}

// Java ExplosionShadowsAI.java:47-62
void ExplosionShadowsAI::check() {
	if (!isDead()) {
		getKnownList().forEachPlayer([this](Player& player) {
			if (player.getEffectController()->hasAbnormalEffect(19502)) {
				runtime::Ptr<Npc> npc = runtime::cast<Npc>(spawn(799657, player.getX(), player.getY(), player.getZ(), player.getHeading()));
				PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_EMOTION(*npc, EmotionType::DIE));
				player.getEffectController()->removeEffect(19502);
			}
		});
		AIActions::deleteOwner(*this);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
