#include "aion/gameserver/handlers/instance/PadmarashkasCaveInstance.h"

#include "aion/gameserver/ai/AI.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/model/geometry/Area.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::handlers::instance {

AION_INSTANCE_HANDLER(PadmarashkasCaveInstance, 320150000);

// Java PadmarashkasCaveInstance.java:31-33
PadmarashkasCaveInstance::PadmarashkasCaveInstance(WorldMapInstance& instance) : GeneralInstanceHandler(instance) {
}

runtime::Ref<PadmarashkasCaveInstance> PadmarashkasCaveInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<PadmarashkasCaveInstance>(instance);
}

// Java PadmarashkasCaveInstance.java:35-66 (getPlayersInside().get(0) of an empty instance is Java's IndexOutOfBoundsException)
void PadmarashkasCaveInstance::onDie(Npc& npc) {
	const int32_t npcId = npc.getNpcId();
	switch (npcId) {
		case 218670:
		case 218671:
		case 218673:
		case 218674:
			if (killedPadmarashkaProtector.incrementAndGet() == 4) {
				killedPadmarashkaProtector.set(0);
				const runtime::Ptr<Npc> padmarashka = getNpc(218756);
				if (padmarashka != nullptr && !padmarashka->isDead()) {
					padmarashka->getEffectController()->unsetAbnormal(AbnormalState::SLEEP);
					// padmarashka.getEffectController().broadCastEffects(0);
					SkillEngine::getInstance().getSkill(*padmarashka, 19187, 55, runtime::Ptr<VisibleObject>(padmarashka))->useNoAnimationSkill();
					padmarashka->getEffectController()->removeEffect(19186); // skill should handle this TODO: fix
					ThreadPoolManager::getInstance().schedule({this}, [this, padmarashka = runtime::Ref<Npc>(padmarashka)] {
						std::vector<runtime::Ptr<Player>> players = instance->getPlayersInside();
						if (players.empty())
							throw runtime::IndexOutOfBoundsException("Index 0 out of bounds for length 0");
						padmarashka->getAi().onCreatureEvent(::aion::gameserver::ai::event::AIEventType::CREATURE_AGGRO, *players[0]);
					}, 1000);
				}
			}
			break;
		case 282613:
		case 282614:
			if (killedEggs.incrementAndGet() == 20) { // TODO: find value
				const runtime::Ptr<Npc> padmarashka = getNpc(218756);
				if (padmarashka != nullptr && !padmarashka->isDead()) {
					SkillEngine::getInstance().applyEffectDirectly(20101, *padmarashka, *padmarashka);
				}
			}
			break;
	}
}

// Java PadmarashkasCaveInstance.java:68-72
void PadmarashkasCaveInstance::onEnterZone(Player& player, world::zone::ZoneInstance& zone) {
	if (zone.getAreaTemplate()->getZoneName() == ZoneName::get("PADMARASHKAS_NEST_320150000") && moviePlayed.compareAndSet(false, true))
		PacketSendUtility::broadcastToMap(*instance, network::aion::serverpackets::SM_PLAY_MOVIE(false, 0, 0, 488, true));
}

// Java PadmarashkasCaveInstance.java:74-78
void PadmarashkasCaveInstance::onInstanceDestroy() {
	moviePlayed.set(false);
	killedPadmarashkaProtector.set(0);
}

} // namespace aion::gameserver::handlers::instance
