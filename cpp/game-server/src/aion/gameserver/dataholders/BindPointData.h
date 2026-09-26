#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/BindPointData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.BindPointData.
 * <p>
 * C++: the index points into the bound `bplist` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author avol
 */
class BindPointData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/BindPointData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::BindPointTemplate*> bindplistData;

public:
	int32_t size() const;

	/** @return the bind point of the npc, nullptr (Java null) if there is none */
	const model::templates::BindPointTemplate* getBindPointTemplate(int32_t npcId) const;
};

} // namespace aion::gameserver::dataholders
