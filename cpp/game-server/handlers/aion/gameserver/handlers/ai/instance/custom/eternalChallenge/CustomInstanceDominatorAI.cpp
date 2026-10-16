#include "aion/gameserver/handlers/ai/instance/custom/eternalChallenge/CustomInstanceDominatorAI.h"

#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/custom/instance/CustomInstanceRankEnumInfo.h"
#include "aion/gameserver/custom/instance/CustomInstanceService.h"
#include "aion/gameserver/custom/instance/RoahCustomInstanceHandler.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORCED_MOVE.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge {

using gameserver::custom::instance::CustomInstanceRankEnum;
using gameserver::custom::instance::getMinRank;

AION_AI(CustomInstanceDominatorAI, "custom_instance_dominator");

// Java CustomInstanceDominatorAI.java:37-50
void CustomInstanceDominatorAI::handleSpawned() {
	AggressiveNoLootNpcAI::handleSpawned();

	runtime::Ptr<world::WorldMapInstance> wmi = getPosition()->getWorldMapInstance();
	if (runtime::as<gameserver::custom::instance::RoahCustomInstanceHandler>(wmi->getInstanceHandler()) == nullptr)
		return;

	challengerObjId.set(wmi->getRegisteredObjects().iterator().next());
	challengerRank.set(gameserver::custom::instance::CustomInstanceService::getInstance().loadOrCreateRank(challengerObjId.get()).getRank());

	debuffTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { debuffTarget(); }, 1000));
	getOwner().setSeeState(CreatureSeeState::SEARCH2);
}

// Java CustomInstanceDominatorAI.java:52-62
AttackIntention CustomInstanceDominatorAI::chooseAttackIntention() {
	runtime::Ptr<VisibleObject> target = getTarget();
	if (!isDead() && target != nullptr) {
		if (!GeoService::getInstance().canSee(getOwner(), *target)) {
			world::World::getInstance().updatePosition(getOwner(), target->getX(), target->getY(), target->getZ(), int8_t{30});
			PacketSendUtility::broadcastPacketAndReceive(getOwner(), network::aion::serverpackets::SM_FORCED_MOVE(getOwner(), getOwner()));
		}
	}
	return AggressiveNoLootNpcAI::chooseAttackIntention();
}

// Java CustomInstanceDominatorAI.java:64-110
void CustomInstanceDominatorAI::debuffTarget() {
	if (!isDead()) {
		if (getOwner().getSkillCoolDowns() != nullptr)
			getOwner().getSkillCoolDowns()->clear(); // Make CD rank-dependent, not skill-dependent

		const int32_t rank = challengerRank.get();
		if (rank >= getMinRank(CustomInstanceRankEnum::ANCIENT))
			getOwner().queueSkill(3539, 65, 0); // Ignite Aether

		if (rank >= getMinRank(CustomInstanceRankEnum::CERANIUM))
			getOwner().queueSkill(618, 65, 0); // Ankle Snare
		else if (rank >= getMinRank(CustomInstanceRankEnum::GOLD))
			getOwner().queueSkill(1328, 65, 0); // Restraint (^)

		if (rank >= getMinRank(CustomInstanceRankEnum::ANCIENT_PLUS))
			getOwner().queueSkill(3775, 65, 0); // Fear
		else if (rank >= getMinRank(CustomInstanceRankEnum::GOLD))
			getOwner().queueSkill(1417, 65, 0); // Curse Tree (^)

		// SILVER:
		getOwner().queueSkill(3581, 65, 0); // Withering Gloom

		if (rank >= getMinRank(CustomInstanceRankEnum::PLATINUM))
			getOwner().queueSkill(3574, 65, 0); // Shackle of Vulnerability
		else // SILVER:
			getOwner().queueSkill(3780, 65, 0); // Root of Enervation (^)

		if (rank >= getMinRank(CustomInstanceRankEnum::MITHRIL)) {
			if (isPhysicalTarget())
				getOwner().queueSkill(3571, 65, 0); // Body Root
			else
				getOwner().queueSkill(3572, 65, 0); // Sigil of Silence
		}

		if (rank >= getMinRank(CustomInstanceRankEnum::CERANIUM)) {
			if (isPhysicalTarget())
				getOwner().queueSkill(4135, 65, 0); // Blinding Light
			else
				getOwner().queueSkill(1336, 65, 0); // Curse of Weakness
		}

		if (rank >= getMinRank(CustomInstanceRankEnum::ANCIENT))
			getOwner().queueSkill(4490, 65, 0); // Paralysis Resonation

		debuffTask.set(ThreadPoolManager::getInstance().schedule({this}, [this] { debuffTarget(); }, 30000 - rank * 1000 / 3)); // 30s ... 20s
	}
}

/** Java: getOwner().getTarget() != null && getOwner().getTarget() instanceof Player && ((Player) (getOwner().getTarget())).getPlayerClass().isPhysicalClass() */
bool CustomInstanceDominatorAI::isPhysicalTarget() {
	runtime::Ptr<Player> targetPlayer = runtime::as<Player>(getOwner().getTarget());
	return targetPlayer != nullptr && model::isPhysicalClass(targetPlayer->getPlayerClass());
}

// Java CustomInstanceDominatorAI.java:112-115
void CustomInstanceDominatorAI::cancelTasks() {
	runtime::Ptr<runtime::Future> task = debuffTask.get();
	if (task != nullptr && !task->isCancelled())
		task->cancel(true);
}

// Java CustomInstanceDominatorAI.java:117-121
void CustomInstanceDominatorAI::handleDied() {
	cancelTasks();
	AggressiveNoLootNpcAI::handleDied();
}

// Java CustomInstanceDominatorAI.java:123-127
void CustomInstanceDominatorAI::handleDespawned() {
	cancelTasks();
	AggressiveNoLootNpcAI::handleDespawned();
}

// Java CustomInstanceDominatorAI.java:129-139
void CustomInstanceDominatorAI::handleBackHome() {
	AggressiveNoLootNpcAI::handleBackHome();
	if (challengerObjId.get() != 0) {
		ThreadPoolManager::getInstance().schedule({this}, [this] {
			runtime::Ptr<Player> challenger = getPosition()->getWorldMapInstance()->getPlayer(challengerObjId.get());
			if (challenger != nullptr && !getOwner().getAggroList().isHating(*challenger) && getOwner().canSee(runtime::Ptr<VisibleObject>(*challenger)))
				getOwner().getAggroList().addHate(*challenger, 1000);
		}, 1000);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge
