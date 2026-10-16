#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::instance::abyss {

/**
 * Unstable Splinterpath (300600000): the unstable version of the Abyssal Splinter - the bosses' fragments and treasure boxes, Rukril and
 * Ebonsoul, the pieces, the waterworms, the huge aether fragments, the artifacts and the two Yamennes with the exit buff.
 * <p>
 * Java: data/handlers/instance/abyss/UnstableSplinterpathInstance.java, @InstanceID(300220000). C++: the scheduled lambda is pinned to the handler.
 *
 * @author zhkchi, vlog, Luzien, Cheatkiller
 */
class UnstableSplinterpathInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> destroyedFragments{};
	runtime::Field<int32_t> killedPazuzuWorms{};

protected:
	explicit UnstableSplinterpathInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<UnstableSplinterpathInstance> create(WorldMapInstance& instance);

	void onSpawn(VisibleObject& object) override;

	void onDie(Npc& npc) override;

	void onInstanceDestroy() override;

	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

private:
	void spawnPazuzuFragment();

	void spawnPazuzuTreasureBoxes();

	void spawnKaluvaFragment();

	void spawnKaluvaTreasureBoxes();

	void spawnDayshadeFragment();

	void spawnDayshadeTreasureBoxes();

	void spawnYamennesTreasureBoxes(int32_t npcId);

	void deleteSummons();

	void onFragmentKill();
};

} // namespace aion::gameserver::handlers::instance::abyss
