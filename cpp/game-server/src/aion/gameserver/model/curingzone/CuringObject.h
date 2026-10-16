#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/curingzone/fwd.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/templates/curingzones/fwd.h"
#include "aion/gameserver/model/templates/fwd.h"
#include "aion/gameserver/model/templates/spawns/fwd.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::model::curingzone {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author xTz
 */
class CuringObject : public gameobjects::VisibleObject {
	AION_MAKE_REF_FRIEND
private:
	const templates::curingzones::CuringTemplate* template_;
	const float range;

protected:
	CuringObject(CreateKey key, const templates::curingzones::CuringTemplate* template_, int32_t instanceId);

public:
	const templates::curingzones::CuringTemplate* getTemplate() const { return this->template_; }

	std::string getName() override;

	float getRange() const { return this->range; }

	void spawn();

protected:
	~CuringObject() override;
};

} // namespace aion::gameserver::model::curingzone
