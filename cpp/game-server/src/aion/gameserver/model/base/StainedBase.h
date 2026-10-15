#pragma once

#include <cstdint>

#include "aion/gameserver/model/base/Base.h"
#include "aion/gameserver/model/base/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::base {

/**
 * Java: model.base.StainedBase (a Base<StainedBaseLocation>): a stained base; three bases of one colour held by one race get the enhanced guardians and outriders.
 *
 * @author Estrayl
 */
class StainedBase : public Base {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::FutureRef> enhancedSpawnTask{};

protected:
	explicit StainedBase(StainedBaseLocation& loc);

public:
	static runtime::Ref<StainedBase> create(StainedBaseLocation& loc);

	/** Java: Base<StainedBaseLocation>.getLocation() - the base's location as the StainedBaseLocation it is */
	runtime::Ptr<StainedBaseLocation> getLocation() const;

	void scheduleEnhancedSpawns();

	void deactivateEnhancedSpawns();

	BaseColorType getColor();

protected:
	int32_t getAssaultDelay() override;

	int32_t getAssaultDespawnDelay() override;

	int32_t getBossSpawnDelay() override;

	int32_t getNpcSpawnDelay() override;

	void handleStop() override;

	~StainedBase() override;
};

} // namespace aion::gameserver::model::base
