#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/town/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ViAl
 */
class SM_TOWNS_LIST : public AionServerPacket {
private:
	std::unordered_map<int32_t, runtime::Ref<model::town::Town>> towns{};
public:
	explicit SM_TOWNS_LIST(const std::unordered_map<int32_t, runtime::Ptr<model::town::Town>>& towns);
	~SM_TOWNS_LIST() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
