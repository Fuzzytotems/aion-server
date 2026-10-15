#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance {

/**
 * Danuar Mysticarium (300480000): the opened doors release their prisoners; the first mini game's portal starts its 3 minute timer, whose end
 * clears the room and spawns the way on; the exits. (Java's class comment describes the three mini games; the second and third are its TODO.)
 * <p>
 * Java: data/handlers/instance/DanuarMysticariumInstance.java, @InstanceID(300480000). C++: the scheduled lambdas are pinned to the handler.
 *
 * @author Yeats
 */
class DanuarMysticariumInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::FutureRef>>> tasks{};

protected:
	explicit DanuarMysticariumInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<DanuarMysticariumInstance> create(WorldMapInstance& instance);

	void onOpenDoor(int32_t door) override;

	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

private:
	void startTasks();

	void cancelTasks();

public:
	void onInstanceDestroy() override;

	void leaveInstance(Player& player) override;
};

} // namespace aion::gameserver::handlers::instance
