#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <initializer_list>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance {

/**
 * Dragon Lord's Refuge (300520000): Tiamat's dialogue with Kahrun, Calindi, Tiamat's dragon form and her four incarnations, the empyrean lord
 * of the first entry's race, Tiamat's weakened form with its 30 minute countdown, and the end with its chest and exit.
 * <p>
 * Java: data/handlers/instance/DragonLordsRefugeInstance.java, @InstanceID(300520000). AnguishedDragonLordsRefugeInstance extends it.
 *
 * @author Cheatkiller, Luzien, Estrayl
 */
class DragonLordsRefugeInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<runtime::Ref<WorldPosition>> respawnLocations{AION_LOCK_CLASS(DragonLordsRefugeInstance::respawnLocations)};

protected:
	runtime::AtomicInteger raceId{AION_LOCK_CLASS(DragonLordsRefugeInstance::raceId), 10};
	runtime::AtomicInteger incarnationKills{AION_LOCK_CLASS(DragonLordsRefugeInstance::incarnationKills)};
	runtime::AtomicInteger progress{AION_LOCK_CLASS(DragonLordsRefugeInstance::progress)};
	runtime::Field<runtime::FutureRef> failTask{};

	explicit DragonLordsRefugeInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<DragonLordsRefugeInstance> create(WorldMapInstance& instance);

	void onSpawn(VisibleObject& object) override;

	void onStartEffect(runtime::Ptr<Effect> effect) override;

	void onEndEffect(Effect& effect) override;

private:
	void endInstance(std::initializer_list<int32_t> despawnExceptions);

public:
	using GeneralInstanceHandler::onDie;

	void onDie(Npc& npc) override;

private:
	void handlePhaseProgress();

	void handleDialogueEvent(Npc& tiamat);

protected:
	virtual void spawnTiamat();

	virtual void spawnCalindi();

public:
	void onEnterInstance(Player& player) override;

	void onCreatureDetected(Npc& detector, Creature& detected) override;

	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

	void onInstanceDestroy() override;

	bool onReviveEvent(Player& player) override;
};

} // namespace aion::gameserver::handlers::instance
