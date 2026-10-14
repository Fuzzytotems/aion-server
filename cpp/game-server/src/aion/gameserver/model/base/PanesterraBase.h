#pragma once

#include <cstdint>

#include "aion/gameserver/model/base/Base.h"
#include "aion/gameserver/model/base/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::base {

/**
 * Java: model.base.PanesterraBase (a Base<PanesterraBaseLocation>): a base of Panesterra, assaulted by the Balaur and captured by a faction.
 *
 * @author Estrayl
 */
class PanesterraBase : public Base {
	AION_MAKE_REF_FRIEND

protected:
	explicit PanesterraBase(PanesterraBaseLocation& loc);

public:
	static runtime::Ref<PanesterraBase> create(PanesterraBaseLocation& loc);

	/** Java: Base<PanesterraBaseLocation>.getLocation() - the base's location as the PanesterraBaseLocation it is */
	runtime::Ptr<PanesterraBaseLocation> getLocation() const;

	BaseOccupier getOccupier(runtime::Ptr<gameobjects::Creature> bossKiller) override;
	using Base::getOccupier;

protected:
	int32_t getAssaultDelay() override;

	int32_t getAssaultDespawnDelay() override;

	int32_t getBossSpawnDelay() override;

	int32_t getNpcSpawnDelay() override;

	BaseOccupier chooseAssaultRace() override;

	~PanesterraBase() override;
};

} // namespace aion::gameserver::model::base
