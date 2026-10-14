#pragma once

#include <cstdint>

#include "aion/gameserver/model/base/Base.h"
#include "aion/gameserver/model/base/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::base {

/**
 * Java: model.base.CasualBase (a Base<BaseLocation>): a casual base, started at boot and recaptured by its assaults.
 *
 * @author Estrayl
 */
class CasualBase : public Base {
	AION_MAKE_REF_FRIEND

protected:
	explicit CasualBase(BaseLocation& loc);

public:
	static runtime::Ref<CasualBase> create(BaseLocation& loc);

protected:
	int32_t getAssaultDelay() override;

	int32_t getAssaultDespawnDelay() override;

	int32_t getBossSpawnDelay() override;

	int32_t getNpcSpawnDelay() override;

	~CasualBase() override;
};

} // namespace aion::gameserver::model::base
