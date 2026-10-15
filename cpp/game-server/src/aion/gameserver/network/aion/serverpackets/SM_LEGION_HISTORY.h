#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/team/legion/LegionHistoryAction_Type.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * S0c declaration header (hub-headers.md §12). C++ difference: Java's `LegionHistoryAction.Type` (an enum nested in an enum) is the
 * generated `LegionHistoryAction_Type`.
 *
 * @author Simple, KID, xTz
 */
class SM_LEGION_HISTORY : public AionServerPacket {
private:
	static constexpr int32_t ENTRIES_PER_PAGE = 8;
	int32_t totalEntries{};
	int32_t page{};
	std::vector<runtime::Ref<model::team::legion::LegionHistoryEntry>> pageEntries{};
	model::team::legion::LegionHistoryAction_Type type{};

public:
	SM_LEGION_HISTORY(const std::vector<runtime::Ptr<model::team::legion::LegionHistoryEntry>>& history,
		model::team::legion::LegionHistoryAction_Type type);
	SM_LEGION_HISTORY(const std::vector<runtime::Ptr<model::team::legion::LegionHistoryEntry>>& history, int32_t page,
		model::team::legion::LegionHistoryAction_Type type);
	~SM_LEGION_HISTORY() override;

protected:
	void writeImpl(AionConnection* con) override;

private:
	std::vector<runtime::Ptr<model::team::legion::LegionHistoryEntry>> findEntriesForCurrentPage(
		const std::vector<runtime::Ptr<model::team::legion::LegionHistoryEntry>>& legionHistory);
};

} // namespace aion::gameserver::network::aion::serverpackets
