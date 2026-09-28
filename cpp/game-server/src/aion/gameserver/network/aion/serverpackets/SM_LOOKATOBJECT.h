#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author alexa026
 */
class SM_LOOKATOBJECT : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::VisibleObject> visibleObject{};
	int32_t targetObjectId{};
	int32_t heading{};
public:
	explicit SM_LOOKATOBJECT(model::gameobjects::VisibleObject& visibleObject);
	~SM_LOOKATOBJECT() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
