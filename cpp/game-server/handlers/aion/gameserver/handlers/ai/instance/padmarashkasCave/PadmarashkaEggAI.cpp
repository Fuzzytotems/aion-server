#include "aion/gameserver/handlers/ai/instance/padmarashkasCave/PadmarashkaEggAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::padmarashkasCave {

AION_AI(PadmarashkaEggAI, "padmarashkaegg");

// Java PadmarashkaEggAI.java:31-37
void PadmarashkaEggAI::handleDied() {
	runtime::Ptr<Npc> guard = protector.get();
	if (guard != nullptr && !guard->isDead()) {
		skillengine::SkillEngine::getInstance().getSkill(*guard, 20176, 55, runtime::Ptr<VisibleObject>(guard))->useNoAnimationSkill(); // apply wrath buff
	}
	NpcAI::handleDied();
}

// Java PadmarashkaEggAI.java:39-68 (Java's casts of the spawns to Npc: the protectors are npcs)
void PadmarashkaEggAI::handleAttack(runtime::Ptr<Creature> creature) {
	NpcAI::handleAttack(creature);
	if (!isSmallEggProtectorSpawned && this->getNpcId() == 282613) {
		switch (commons::utils::Rnd::get(1, 6)) {
			case 1:
				protector = runtime::Ref<Npc>(runtime::cast<Npc>(spawn(282715, 579.415f, 168.109f, 66.000f, int8_t{0})));
				break;
			case 2:
				protector = runtime::Ref<Npc>(runtime::cast<Npc>(spawn(282715, 581.316f, 157.520f, 66.000f, int8_t{0})));
				break;
			case 3:
				protector = runtime::Ref<Npc>(runtime::cast<Npc>(spawn(282715, 575.073f, 147.338f, 66.000f, int8_t{0})));
				break;
			case 4:
				protector = runtime::Ref<Npc>(runtime::cast<Npc>(spawn(282715, 585.119f, 150.989f, 66.000f, int8_t{0})));
				break;
			case 5:
				protector = runtime::Ref<Npc>(runtime::cast<Npc>(spawn(282716, 581.141f, 148.342f, 66.000f, int8_t{0})));
				break;
			case 6:
				protector = runtime::Ref<Npc>(runtime::cast<Npc>(spawn(282716, 584.240f, 142.233f, 66.000f, int8_t{0})));
				break;
		}
		isSmallEggProtectorSpawned = true;
	} else if (!isHugeEggProtectorSpawned && this->getNpcId() == 282614) {
		SpawnEliteCommander(); // Random spawn SpawnEliteCommander to protect Egg
		isHugeEggProtectorSpawned = true;
	}
}

// Java PadmarashkaEggAI.java:70-72
void PadmarashkaEggAI::SpawnEliteCommander() {
	protector = runtime::Ref<Npc>(runtime::cast<Npc>(rndSpawnInRange(282712, 5)));
}

// Java PadmarashkaEggAI.java:74-85
void PadmarashkaEggAI::handleSpawned() {
	NpcAI::handleSpawned();
	switch (this->getNpcId()) {
		case 282613:
			smallEggSpawn();
			break;
		case 282614:
			hugeEggSpawn();
			break;
	}
}

// Java PadmarashkaEggAI.java:87-95
void PadmarashkaEggAI::smallEggSpawn() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead() && getOwner().isSpawned()) {
			AIActions::deleteOwner(*this);
			attackPlayer(*runtime::cast<Npc>(spawn(282616, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0})));
		}
	}, 60000); // TODO: Need right value
}

// Java PadmarashkaEggAI.java:97-104
void PadmarashkaEggAI::hugeEggSpawn() {
	ThreadPoolManager::getInstance().schedule({this}, [this] {
		if (!isDead() && getOwner().isSpawned()) {
			AIActions::deleteOwner(*this);
			attackPlayer(*runtime::cast<Npc>(spawn(282620, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0})));
		}
	}, 120000); // TODO: Need right value
}

// Java PadmarashkaEggAI.java:106-121 (the anonymous Runnable captures the npc)
void PadmarashkaEggAI::attackPlayer(Npc& npc) {
	ThreadPoolManager::getInstance().schedule({this}, [this, npc = runtime::Ref<Npc>(npc)] {
		runtime::Ptr<Npc> padma = getOwner().getPosition()->getWorldMapInstance()->getNpc(218756);
		if (padma != nullptr) {
			npc->setTarget(padma->getTarget());
			npc->getAi().setStateIfNot(AIState::WALKING);
			npc->setState(CreatureState::ACTIVE, true);
			npc->getMoveController()->moveToTargetObject();
			PacketSendUtility::broadcastPacket(*npc, network::aion::serverpackets::SM_EMOTION(*npc, EmotionType::CHANGE_SPEED, 0, npc->getObjectId()));
		}
	}, 1000);
}

} // namespace aion::gameserver::handlers::ai::instance::padmarashkasCave
