#pragma once

#include <cstdint>

#include "aion/gameserver/model/base/BaseLocation.h"
#include "aion/gameserver/model/base/fwd.h"
#include "aion/gameserver/model/templates/base/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::base {

/**
 * Java: model.base.SiegeBaseLocation (a BaseLocation): the location of a base that belongs to a siege (BaseType.SIEGE).
 *
 * @author Estrayl
 */
class SiegeBaseLocation : public BaseLocation {
	AION_MAKE_REF_FRIEND

protected:
	explicit SiegeBaseLocation(const templates::base::BaseTemplate* value);

public:
	static runtime::Ref<SiegeBaseLocation> create(const templates::base::BaseTemplate* value);

protected:
	~SiegeBaseLocation() override;
};

} // namespace aion::gameserver::model::base
