#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::instance::abyss {

/**
 * Abyssal Splinter (300220000): the bosses' fragments and treasure boxes, Rukril and Ebonsoul who return unless both die within a minute,
 * the pieces that lift their shields, the Pazuzu worms, the three huge aether fragments that turn the easy artifact into the hard one, the
 * quest items and the two Yamennes.
 * <p>
 * Java: data/handlers/instance/abyss/AbyssalSplinterInstance.java, @InstanceID(300220000). C++: the scheduled lambda is pinned to the handler.
 *
 * @author zhkchi, vlog, Luzien
 */
class AbyssalSplinterInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> destroyedFragments{};
	runtime::Field<int32_t> killedPazuzuWorms{};

protected:
	explicit AbyssalSplinterInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<AbyssalSplinterInstance> create(WorldMapInstance& instance);

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
