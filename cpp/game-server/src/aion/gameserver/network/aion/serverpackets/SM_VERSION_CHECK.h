#pragma once

#include <cstdint>

#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author -Nemesiss-, Novo, cura, Neon
 */
class SM_VERSION_CHECK : public AionServerPacket {
public:
	static constexpr int32_t INTERNAL_VERSION = 207;
private:
	int32_t version{};
	model::EventTheme cityDecoration{};
public:
	explicit SM_VERSION_CHECK(model::EventTheme cityDecoration);
	SM_VERSION_CHECK(int32_t version, model::EventTheme cityDecoration);
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
