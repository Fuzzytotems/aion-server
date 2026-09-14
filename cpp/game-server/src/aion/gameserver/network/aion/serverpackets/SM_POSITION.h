#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Instantly moves the object to the given position and cancels movement on client side if it is a player.
 *
 * @author Sweetkr
 */
class SM_POSITION : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::VisibleObject> object{};
public:
	explicit SM_POSITION(model::gameobjects::VisibleObject& object);
	~SM_POSITION() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
