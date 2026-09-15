#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/TeleLocationData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.TeleLocationData.
 * <p>
 * C++: the index points into the bound `tlist` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author orz
 */
class TeleLocationData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/TeleLocationData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::teleport::TelelocationTemplate*> loctlistData;

public:
	int32_t size() const;

	/** @return the teleport location, nullptr (Java null) if there is none */
	const model::templates::teleport::TelelocationTemplate* getTelelocationTemplate(int32_t id) const;
};

} // namespace aion::gameserver::dataholders
