#pragma once

#include <cstdint>

#include "aion/gameserver/model/base/BaseColorType.h"
#include "aion/gameserver/model/base/BaseLocation.h"
#include "aion/gameserver/model/base/fwd.h"
#include "aion/gameserver/model/templates/base/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::base {

/**
 * Java: model.base.StainedBaseLocation (a BaseLocation): the location of a stained base of Kaldor and Levinshor, with its colour.
 *
 * @author Estrayl
 */
class StainedBaseLocation : public BaseLocation {
	AION_MAKE_REF_FRIEND
private:
	const BaseColorType color;

protected:
	explicit StainedBaseLocation(const templates::base::BaseTemplate* value);

public:
	static runtime::Ref<StainedBaseLocation> create(const templates::base::BaseTemplate* value);

	BaseColorType getColor() const { return this->color; }

protected:
	~StainedBaseLocation() override;
};

} // namespace aion::gameserver::model::base
