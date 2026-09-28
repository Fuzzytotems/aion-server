#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/siege/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Sarynth, Neon
 */
class SM_SIEGE_LOCATION_INFO : public AionServerPacket {
private:
	int32_t infoType{};
	std::unordered_map<int32_t, runtime::Ref<model::siege::SiegeLocation>> locations{};
public:
	SM_SIEGE_LOCATION_INFO();
	explicit SM_SIEGE_LOCATION_INFO(model::siege::SiegeLocation& loc);
	~SM_SIEGE_LOCATION_INFO() override;

	/** C++ only: writeImpl reads the connection, so every recipient gets its own serialization (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
