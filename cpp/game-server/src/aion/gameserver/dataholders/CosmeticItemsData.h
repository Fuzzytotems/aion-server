#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>

#include "aion/gameserver/dataholders/CosmeticItemsData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.CosmeticItemsData.
 * <p>
 * C++: the index points into the bound `templates` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author xTz
 */
class CosmeticItemsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/CosmeticItemsData.xml.inc"
private:
	std::map<std::string, const model::templates::cosmeticitems::CosmeticItemTemplate*, std::less<>> cosmeticItemTemplates;

public:
	int32_t size() const;

	/** @return the template with the cosmetic name, nullptr (Java null) if there is none */
	const model::templates::cosmeticitems::CosmeticItemTemplate* getCosmeticItemsTemplate(std::string_view str) const;
};

} // namespace aion::gameserver::dataholders
