#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author xTz, Source
 */
class SM_SHIELD_EFFECT : public AionServerPacket {
private:
	std::vector<runtime::Ref<model::siege::SiegeLocation>> locations{};
public:
	explicit SM_SHIELD_EFFECT(const std::vector<runtime::Ptr<model::siege::SiegeLocation>>& locations);
	explicit SM_SHIELD_EFFECT(int32_t location);
	~SM_SHIELD_EFFECT() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
