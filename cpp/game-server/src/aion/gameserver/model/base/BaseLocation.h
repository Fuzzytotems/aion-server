#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/base/fwd.h"
#include "aion/gameserver/model/templates/base/fwd.h"

namespace aion::gameserver::model::base {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Source
 */
class BaseLocation : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
protected:
	runtime::Field<const templates::base::BaseTemplate*> template_{};
	runtime::Field<BaseType> type{};
	runtime::Field<BaseOccupier> occupier{};

	explicit BaseLocation(const templates::base::BaseTemplate* template_);

public:
	static runtime::Ref<BaseLocation> create(const templates::base::BaseTemplate* value);

	int32_t getId();

	int32_t getWorldId();

	BaseType getType() const { return this->type.get(); }

	BaseOccupier getOccupier() const { return this->occupier.get(); }

	void setOccupier(BaseOccupier value) { this->occupier.set(value); }

	const templates::base::BaseTemplate* getTemplate() const;

protected:
	~BaseLocation() override;
};

} // namespace aion::gameserver::model::base
