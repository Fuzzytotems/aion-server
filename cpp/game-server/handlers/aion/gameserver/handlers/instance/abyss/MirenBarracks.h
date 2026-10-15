#pragma once

#include "aion/gameserver/handlers/instance/abyss/AbstractInnerUpperAbyssInstance.h"

namespace aion::gameserver::handlers::instance::abyss {

/**
 * MirenBarracks (301290000): an inner upper abyss siege keep - its doors open as their guards die, the end boss's death brings the chests.
 * <p>
 * Java: data/handlers/instance/abyss/MirenBarracks.java, @InstanceID(301290000).
 *
 * @author Estrayl
 */
class MirenBarracks : public AbstractInnerUpperAbyssInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit MirenBarracks(WorldMapInstance& instance);

	int32_t getBossId() override;

	int32_t getChestId() override;

	int32_t getDoorId() override;

	int32_t getKeymasterId() override;

	int32_t getTimerNpcId() override;

public:
	static runtime::Ref<MirenBarracks> create(WorldMapInstance& instance);

	void onDie(Npc& npc) override;
};

} // namespace aion::gameserver::handlers::instance::abyss
