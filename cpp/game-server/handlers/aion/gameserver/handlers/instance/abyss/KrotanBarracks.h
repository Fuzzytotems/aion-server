#pragma once

#include "aion/gameserver/handlers/instance/abyss/AbstractInnerUpperAbyssInstance.h"

namespace aion::gameserver::handlers::instance::abyss {

/**
 * KrotanBarracks (301300000): an inner upper abyss siege keep - its doors open as their guards die, the end boss's death brings the chests.
 * <p>
 * Java: data/handlers/instance/abyss/KrotanBarracks.java, @InstanceID(301300000).
 *
 * @author Estrayl
 */
class KrotanBarracks : public AbstractInnerUpperAbyssInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit KrotanBarracks(WorldMapInstance& instance);

	int32_t getBossId() override;

	int32_t getChestId() override;

	int32_t getDoorId() override;

	int32_t getKeymasterId() override;

	int32_t getTimerNpcId() override;

public:
	static runtime::Ref<KrotanBarracks> create(WorldMapInstance& instance);

	void onDie(Npc& npc) override;
};

} // namespace aion::gameserver::handlers::instance::abyss
