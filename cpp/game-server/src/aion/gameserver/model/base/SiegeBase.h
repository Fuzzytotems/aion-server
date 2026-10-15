#pragma once

#include <cstdint>

#include "aion/gameserver/model/base/Base.h"
#include "aion/gameserver/model/base/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::base {

/**
 * Java: model.base.SiegeBase (a Base<SiegeBaseLocation>): a base that belongs to a siege, started and stopped by its fortress.
 *
 * @author Estrayl
 */
class SiegeBase : public Base {
	AION_MAKE_REF_FRIEND

protected:
	explicit SiegeBase(SiegeBaseLocation& loc);

public:
	static runtime::Ref<SiegeBase> create(SiegeBaseLocation& loc);

	/** Java: Base<SiegeBaseLocation>.getLocation() - the base's location as the SiegeBaseLocation it is */
	runtime::Ptr<SiegeBaseLocation> getLocation() const;

protected:
	int32_t getAssaultDelay() override;

	int32_t getAssaultDespawnDelay() override;

	int32_t getBossSpawnDelay() override;

	int32_t getNpcSpawnDelay() override;

	~SiegeBase() override;
};

} // namespace aion::gameserver::model::base
