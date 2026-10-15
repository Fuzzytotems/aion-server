#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance {

/**
 * Sauro Supply Base (301130000): the random grave robber and Commander Ranodim, 8 of the 23 chest points with the better chest; the bosses
 * open their doors, the last ones the exits; Guard Captain Ahuradim's three shield generators come back while he lives, one of them turning on
 * him after 40 s and casting 15 s later; the teleports to the last rooms.
 * <p>
 * Java: data/handlers/instance/SauroSupplyBaseInstance.java, @InstanceID(301130000). C++: the scheduled lambdas are pinned to the handler.
 */
class SauroSupplyBaseInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	// Java: = new ArrayList<>(), filled by the static block (SauroSupplyBaseInstance.cpp)
	static inline runtime::ArrayList<runtime::Ref<WorldPosition>> chestPoints{AION_LOCK_CLASS(SauroSupplyBaseInstance::chestPoints)};
	runtime::Field<runtime::FutureRef> scheduledGeneratorTask{};

	static const bool chestPointsFilled;

protected:
	explicit SauroSupplyBaseInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<SauroSupplyBaseInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

	void onDie(Npc& npc) override;

private:
	void startGeneratorTask(int32_t npcId, Npc& ahuradim);

	void cancelTask();

public:
	void onBackHome(Npc& npc) override;

	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

	bool isBoss(Npc& npc) override;
};

} // namespace aion::gameserver::handlers::instance
