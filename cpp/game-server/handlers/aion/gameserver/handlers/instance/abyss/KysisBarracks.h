#pragma once

#include "aion/gameserver/handlers/instance/abyss/AbstractInnerUpperAbyssInstance.h"

namespace aion::gameserver::handlers::instance::abyss {

/**
 * KysisBarracks (301280000): an inner upper abyss siege keep - its doors open as their guards die, the end boss's death brings the chests.
 * <p>
 * Java: data/handlers/instance/abyss/KysisBarracks.java, @InstanceID(301280000).
 *
 * @author Estrayl
 */
class KysisBarracks : public AbstractInnerUpperAbyssInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit KysisBarracks(WorldMapInstance& instance);

	int32_t getBossId() override;

	int32_t getChestId() override;

	int32_t getDoorId() override;

	int32_t getKeymasterId() override;

	int32_t getTimerNpcId() override;

public:
	static runtime::Ref<KysisBarracks> create(WorldMapInstance& instance);

	void onDie(Npc& npc) override;
};

} // namespace aion::gameserver::handlers::instance::abyss
