#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"
#include "aion/gameserver/model/house/HouseBids.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Rolandas, Neon
 */
class SM_HOUSE_BIDS : public AionServerPacket {
public:
	static constexpr int32_t STATIC_BODY_SIZE = 28;
	/** Java: (bid) -> 44 */
	static const runtime::PinnedCallback<int32_t(model::house::HouseBids&)> DYNAMIC_BODY_PART_SIZE_CALCULATOR;

private:
	bool isFirst{};
	bool isLast{};
	std::vector<runtime::Ref<model::house::HouseBids>> houseBids{};

public:
	SM_HOUSE_BIDS(bool isFirstPacket, bool isLastPacket, const std::vector<runtime::Ptr<model::house::HouseBids>>& houseBids);
	~SM_HOUSE_BIDS() override;

	/** writeImpl reads the connection: serialized per recipient (runtime-architecture.md §8.3) */
	Recipients recipients() const noexcept override { return Recipients::PER_RECIPIENT; }

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
