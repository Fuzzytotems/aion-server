#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ATracer
 */
class SM_GATHERABLE_INFO : public AionServerPacket {
private:
	runtime::Ref<model::gameobjects::VisibleObject> visibleObject{};

public:
	explicit SM_GATHERABLE_INFO(model::gameobjects::VisibleObject& visibleObject);
	~SM_GATHERABLE_INFO() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
