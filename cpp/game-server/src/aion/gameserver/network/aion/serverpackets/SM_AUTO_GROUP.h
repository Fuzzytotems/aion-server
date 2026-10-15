#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author SheppeR, Guapo, nrg, Estrayl
 */
class SM_AUTO_GROUP : public AionServerPacket {
public:
	static constexpr int8_t WND_ENTRY_ICON = 6;

private:
	int32_t maskId{};
	int32_t mapId{};
	int32_t messageId{};
	int32_t titleId{};
	int32_t windowId{};
	int32_t requestTypeId{};
	bool close{};
	std::string name{};

public:
	explicit SM_AUTO_GROUP(int32_t maskId);
	SM_AUTO_GROUP(int32_t maskId, int32_t windowId);
	SM_AUTO_GROUP(int32_t maskId, int32_t windowId, bool close);
	SM_AUTO_GROUP(int32_t maskId, int32_t windowId, int32_t requestTypeId, std::string_view name);

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
