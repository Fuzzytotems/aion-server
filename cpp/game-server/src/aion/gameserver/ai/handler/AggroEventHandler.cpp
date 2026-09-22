#include "aion/gameserver/ai/handler/AggroEventHandler.h"

#include <optional>

#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/TribeClassInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/npc/NpcTemplateType.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/services/TribeRelationService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::ai::handler {

using event::AIEventType;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::VisibleObject;
using model::gameobjects::player::Player;
using model::templates::npc::NpcTemplateType;
using utils::PositionUtil;

namespace {

/** Java: owner.getTribe().isGuard() - the implicit null check of the dereference (42 npc_templates have no tribe) */
model::TribeClass requireTribe(Npc& npc) {
	std::optional<model::TribeClass> tribe = npc.getTribe();
	if (!tribe)
		throw runtime::NullPointerException("Cannot invoke \"TribeClass.isGuard()\" because the return value of \"Npc.getTribe()\" is null");
	return *tribe;
}

} // namespace

// Java implements Runnable. A task object of schedule that reads aggressive and target on the pool thread and nulls both in run(), so it is
// RefCounted and retains them (fieldmap K4, hub-headers.md §7.3/§9.3). The null-out is Java's own cycle breaker (AggroEventHandler.java:75-76).
class AggroEventHandler::AggroNotifier final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const bool broadcast;
	runtime::Field<runtime::Ref<Npc>> aggressive{};
	runtime::Field<runtime::Ref<Creature>> target{};

protected:
	AggroNotifier(Npc& aggressiveValue, Creature& targetValue, bool broadcastValue)
		: broadcast(broadcastValue), aggressive(runtime::Ref<Npc>(aggressiveValue)), target(runtime::Ref<Creature>(targetValue)) {}
	~AggroNotifier() override = default;

public:
	/** Java: new AggroNotifier(aggressive, target, broadcast) */
	static runtime::Ref<AggroNotifier> create(Npc& aggressive, Creature& target, bool broadcast) {
		return runtime::makeRef<AggroNotifier>(aggressive, target, broadcast);
	}

	void run() { // @Override of a Java library type
		runtime::Ptr<Npc> npc = aggressive.get(); // Java: NullPointerException once the fields are null
		runtime::Ptr<Creature> hated = target.get();
		npc->getAggroList().addHate(*hated, 1);
		if (broadcast)
			npc->getKnownList().forEachNpc([&npc](Npc& object) { object.getAi().onCreatureEvent(AIEventType::CREATURE_NEEDS_SUPPORT, *npc); });

		aggressive.set(nullptr);
		target.set(nullptr);
	}
};

void AggroEventHandler::onAggro(NpcAI& npcAI, Creature& target) {
	Npc& owner = npcAI.getOwner();
	runtime::Ref<AggroNotifier> notifier = AggroNotifier::create(owner, target, true);
	utils::ThreadPoolManager::getInstance().schedule({&owner, &target}, [notifier] { notifier->run(); }, 500);
	owner.getWorldMapInstance()->getInstanceHandler()->onAggro(owner);
}

bool AggroEventHandler::onCreatureNeedsSupport(NpcAI& npcAI, Creature& creatureAskingForSupport) {
	Npc& owner = npcAI.getOwner();
	runtime::Ptr<Creature> attacker = runtime::as<Creature>(creatureAskingForSupport.getTarget());
	if (!attacker || owner.getAggroList().isHating(*attacker))
		return false;
	if (services::TribeRelationService::canHelpCreature(owner, creatureAskingForSupport) &&
		isInSupportRange(owner, creatureAskingForSupport, *attacker)) {
		runtime::Ref<AggroNotifier> notifier = AggroNotifier::create(owner, *attacker, false);
		utils::ThreadPoolManager::getInstance().schedule({&owner, attacker.get()}, [notifier] { notifier->run(); }, 500);
		return true;
	}
	return false;
}

bool AggroEventHandler::onCreatureNeedsSupportByGuard(NpcAI& npcAI, Creature& creatureAskingForSupport) {
	Npc& owner = npcAI.getOwner();
	if (owner.getNpcTemplateType() != NpcTemplateType::GUARD && !model::isGuard(requireTribe(owner)))
		return false;
	runtime::Ptr<Player> enemy = runtime::as<Player>(creatureAskingForSupport.getTarget());
	if (!enemy || owner.getAggroList().isHating(*enemy))
		return false;
	if (owner.isEnemy(*enemy) && isInSupportRange(owner, creatureAskingForSupport, *enemy)) {
		runtime::Ref<AggroNotifier> notifier = AggroNotifier::create(owner, *enemy, false);
		utils::ThreadPoolManager::getInstance().schedule({&owner, enemy.get()}, [notifier] { notifier->run(); }, 500);
		return true;
	}
	return false;
}

bool AggroEventHandler::isInSupportRange(Npc& npc, Creature& creatureAskingForSupport, Creature& target) {
	int32_t range = npc.getAggroRange() + SUPPORT_RANGE_OFFSET;
	return PositionUtil::isInRange(npc, creatureAskingForSupport, static_cast<float>(range), false) &&
			   world::geo::GeoService::getInstance().canSee(npc, creatureAskingForSupport) ||
		PositionUtil::isInRange(npc, target, static_cast<float>(range), false) && world::geo::GeoService::getInstance().canSee(npc, target);
}

} // namespace aion::gameserver::ai::handler
