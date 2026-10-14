#pragma once

#include <cstdint>

#include "aion/gameserver/model/base/PanesterraBase.h"
#include "aion/gameserver/model/base/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::base {

/**
 * Java: model.base.PanesterraFactionCamp (a PanesterraBase): a faction camp of Panesterra: destroying its soul anchor (the boss) eliminates the camp.
 *
 * @author Estrayl
 */
class PanesterraFactionCamp : public PanesterraBase {
	AION_MAKE_REF_FRIEND

protected:
	explicit PanesterraFactionCamp(PanesterraBaseLocation& loc);

public:
	static runtime::Ref<PanesterraFactionCamp> create(PanesterraBaseLocation& loc);

	BaseOccupier getOccupier(runtime::Ptr<gameobjects::Creature> bossKiller) override;
	using PanesterraBase::getOccupier;

protected:
	int32_t getBossSpawnDelay() override;

	int32_t getNpcSpawnDelay() override;

	~PanesterraFactionCamp() override;
};

} // namespace aion::gameserver::model::base
