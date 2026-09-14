#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Rolandas, Neon, Sykra
 */
class SM_HOUSE_SCRIPTS : public AionServerPacket {
public:
	static constexpr int32_t STATIC_BODY_SIZE = 6;

private:
	/** values seem to not matter, but length does */
	// fieldmap: Java byte[] constant; a constexpr table instead of a RefCounted Array created at static initialization
	static constexpr std::array<int8_t, 8> SCRIPT_PADDING{-51, -51, -51, -51, -51, -51, -51, -51};

public:
	static constexpr int32_t MAX_COMPRESSED_SCRIPT_SIZE =
		AionServerPacket::MAX_USABLE_PACKET_BODY_SIZE - STATIC_BODY_SIZE - 11 - static_cast<int32_t>(SCRIPT_PADDING.size());
	/** Java: script -> script.hasData() ? 11 + script.compressedBytes().length + SCRIPT_PADDING.length : 3 */
	static const runtime::PinnedCallback<int32_t(model::house::PlayerScript&)> DYNAMIC_BODY_PART_SIZE_CALCULATOR;

private:
	int32_t houseAddress{};
	std::vector<runtime::Ref<model::house::PlayerScript>> scripts{};

public:
	SM_HOUSE_SCRIPTS(int32_t houseAddress, runtime::Ptr<model::house::PlayerScript> script);
	SM_HOUSE_SCRIPTS(int32_t houseAddress, const std::vector<runtime::Ptr<model::house::PlayerScript>>& scripts);
	~SM_HOUSE_SCRIPTS() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
