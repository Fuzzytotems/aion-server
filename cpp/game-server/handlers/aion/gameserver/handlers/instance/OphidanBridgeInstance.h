#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance {

/**
 * Ophidan Bridge (300590000): hard mode unless Escapee Asachin dies; the third of the four bosses brings the bridge control and, in hard mode,
 * one of three random end boss pairs (Spirited Velkur in normal mode); the end bosses' deaths open the exit or, too slow, the enrage.
 * <p>
 * Java: data/handlers/instance/OphidanBridgeInstance.java, @InstanceID(300590000). C++: the scheduled lambda is pinned to the handler.
 *
 * @author Yeats
 */
class OphidanBridgeInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::AtomicBoolean isNormalMode{AION_LOCK_CLASS(OphidanBridgeInstance::isNormalMode)}; // instance starts always in hardmode
	runtime::AtomicBoolean used{AION_LOCK_CLASS(OphidanBridgeInstance::used)}; // Java: = new AtomicBoolean()
	runtime::Field<int8_t> bossKills{};
	runtime::Field<int8_t> endBossKills{};
	runtime::Field<runtime::FutureRef> task{};

protected:
	explicit OphidanBridgeInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<OphidanBridgeInstance> create(WorldMapInstance& instance);

	void onInstanceDestroy() override;

	void onDie(Npc& npc) override;

private:
	void scheduleEnrage();

	void spawnRandomBosses();

public:
	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

	bool onReviveEvent(Player& player) override;
};

} // namespace aion::gameserver::handlers::instance
