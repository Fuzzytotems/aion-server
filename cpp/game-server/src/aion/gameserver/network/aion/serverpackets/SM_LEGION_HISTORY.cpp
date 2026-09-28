#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_HISTORY.h"

#include <algorithm>
#include <cstddef>

#include "aion/gameserver/model/team/legion/LegionHistoryEntry.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_HISTORY::SM_LEGION_HISTORY(const std::vector<runtime::Ptr<model::team::legion::LegionHistoryEntry>>& history,
	model::team::legion::LegionHistoryAction_Type typeValue)
	: SM_LEGION_HISTORY(history, 0, typeValue) {
}

SM_LEGION_HISTORY::SM_LEGION_HISTORY(const std::vector<runtime::Ptr<model::team::legion::LegionHistoryEntry>>& history, int32_t pageValue,
	model::team::legion::LegionHistoryAction_Type typeValue)
	: AionServerPacket(opcodeOf<SM_LEGION_HISTORY>), totalEntries(static_cast<int32_t>(history.size())), page(pageValue), type(typeValue) {
	// Java keeps a subList view of the history; the entries of the page are copied (the history is not modified while the packet lives)
	for (const runtime::Ptr<model::team::legion::LegionHistoryEntry>& entry : findEntriesForCurrentPage(history))
		pageEntries.emplace_back(*entry);
}

SM_LEGION_HISTORY::~SM_LEGION_HISTORY() = default;

void SM_LEGION_HISTORY::writeImpl(AionConnection* con) {
	writeD(totalEntries);
	writeD(page); // current page
	writeD(static_cast<int32_t>(pageEntries.size()));
	for (const runtime::Ref<model::team::legion::LegionHistoryEntry>& entry : pageEntries) {
		writeD(entry->epochSeconds());
		writeC(detail::legionHistoryActionId(entry->action()));
		writeC(0); // unk
		writeS(entry->name(), 32);
		writeS(entry->description(), 32);
		writeH(0);
	}
	writeH(static_cast<int32_t>(type)); // Java type.ordinal()
}

std::vector<runtime::Ptr<model::team::legion::LegionHistoryEntry>> SM_LEGION_HISTORY::findEntriesForCurrentPage(
	const std::vector<runtime::Ptr<model::team::legion::LegionHistoryEntry>>& legionHistory) {
	// Java int arithmetic wraps; a wrapped negative start index is rejected like in Java
	int32_t startIndex = static_cast<int32_t>(static_cast<uint32_t>(page) * static_cast<uint32_t>(ENTRIES_PER_PAGE));
	if (startIndex < 0 || static_cast<size_t>(startIndex) >= legionHistory.size())
		return {};
	size_t endIndex = std::min(static_cast<size_t>(startIndex) + static_cast<size_t>(ENTRIES_PER_PAGE), legionHistory.size());
	return {legionHistory.begin() + startIndex, legionHistory.begin() + static_cast<std::ptrdiff_t>(endIndex)};
}

} // namespace aion::gameserver::network::aion::serverpackets
