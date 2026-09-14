#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_HISTORY.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/team/legion/LegionHistoryEntry.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_LEGION_HISTORY::SM_LEGION_HISTORY(const std::vector<runtime::Ptr<model::team::legion::LegionHistoryEntry>>& history,
	model::team::legion::LegionHistoryAction_Type typeValue)
	: SM_LEGION_HISTORY(history, 0, typeValue) {
}

SM_LEGION_HISTORY::SM_LEGION_HISTORY(const std::vector<runtime::Ptr<model::team::legion::LegionHistoryEntry>>& history, int32_t pageValue,
	model::team::legion::LegionHistoryAction_Type typeValue)
	: AionServerPacket(opcodeOf<SM_LEGION_HISTORY>), totalEntries(static_cast<int32_t>(history.size())), page(pageValue), type(typeValue) {
	AION_UNPORTED(); // pageEntries = findEntriesForCurrentPage(history)
}

SM_LEGION_HISTORY::~SM_LEGION_HISTORY() = default;

void SM_LEGION_HISTORY::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<model::team::legion::LegionHistoryEntry>> SM_LEGION_HISTORY::findEntriesForCurrentPage(
	const std::vector<runtime::Ptr<model::team::legion::LegionHistoryEntry>>& legionHistory) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
