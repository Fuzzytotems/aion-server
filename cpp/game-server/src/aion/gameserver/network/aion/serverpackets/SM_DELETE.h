#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * This packet is informing client that some AionObject is no longer visible.
 *
 * @author -Nemesiss-, Neon
 */
class SM_DELETE : public AionServerPacket {
private:
	int32_t objectId{};
	int32_t animationId{};

public:
	explicit SM_DELETE(model::gameobjects::VisibleObject& object);
	SM_DELETE(model::gameobjects::VisibleObject& object, bool inRange);
	SM_DELETE(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation);

private:
	SM_DELETE(model::gameobjects::VisibleObject& object, model::animations::ObjectDeleteAnimation animation, bool inRange);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
