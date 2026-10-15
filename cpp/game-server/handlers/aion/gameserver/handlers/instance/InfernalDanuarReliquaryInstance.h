#pragma once

#include "aion/gameserver/handlers/instance/DanuarReliquaryInstance.h"

namespace aion::gameserver::handlers::instance {

/**
 * Infernal Danuar Reliquary (301360000): Crazed Modor and her clones; the success leaves her standing (no treasure box), the failure clears all.
 * <p>
 * Java: data/handlers/instance/InfernalDanuarReliquaryInstance.java, @InstanceID(301360000).
 *
 * @author Yeats
 */
class InfernalDanuarReliquaryInstance : public DanuarReliquaryInstance {
	AION_MAKE_REF_FRIEND
protected:
	explicit InfernalDanuarReliquaryInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<InfernalDanuarReliquaryInstance> create(WorldMapInstance& instance);

protected:
	int32_t getExitId() override;

	int32_t getTreasureBoxId() override;

	int32_t getEnragedModorId() override;

	int32_t getCursedModorId() override;

	int32_t getRealCloneId() override;

	int32_t getFakeCloneId() override;

	void onInstanceEnd(bool successful) override;
};

} // namespace aion::gameserver::handlers::instance
