#pragma once

#include <cstdint>

#include "aion/gameserver/model/base/BaseLocation.h"
#include "aion/gameserver/model/base/fwd.h"
#include "aion/gameserver/model/templates/base/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::base {

/**
 * Java: model.base.PanesterraBaseLocation (a BaseLocation): the location of a Panesterra base; a faction camp starts at peace.
 *
 * @author Estrayl
 */
class PanesterraBaseLocation : public BaseLocation {
	AION_MAKE_REF_FRIEND

protected:
	explicit PanesterraBaseLocation(const templates::base::BaseTemplate* value);

public:
	static runtime::Ref<PanesterraBaseLocation> create(const templates::base::BaseTemplate* value);

protected:
	~PanesterraBaseLocation() override;
};

} // namespace aion::gameserver::model::base
