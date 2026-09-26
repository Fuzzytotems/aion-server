#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/TradeListData.xml.h"
#include "aion/gameserver/dataholders/detail/LinkedMap.h"
#include "aion/gameserver/model/templates/npc/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.TradeListData.
 * <p>
 * C++: the indexes point into the bound lists, which stay after afterUnmarshal (static-data.md §2.6). getTradeListTemplate() returns the trade
 * list index as a read-only map that iterates in Java's HashMap order (LimitedItemTradeService.start walks it). validateBuyLists gets the npc
 * templates Java passes as `NPC_DATA.getNpcData()`.
 *
 * @author Luno
 */
class TradeListData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/TradeListData.xml.inc"
private:
	/** in Java's HashMap iteration order */
	detail::LinkedMap<int32_t, const model::templates::tradelist::TradeListTemplate*> npctlistData;
	std::unordered_map<int32_t, const model::templates::tradelist::TradeListTemplate*> npcTradeInlistData;
	std::unordered_map<int32_t, const model::templates::tradelist::TradeListTemplate*> npcPurchaseTemplateData;

public:
	int32_t size() const;

	/**
	 * Returns an {@link TradeListTemplate} object with given id.
	 *
	 * @return TradeListTemplate object containing data about NPC with that id, nullptr (Java null) if there is none.
	 */
	const model::templates::tradelist::TradeListTemplate* getTradeListTemplate(int32_t id) const;

	const model::templates::tradelist::TradeListTemplate* getTradeInListTemplate(int32_t id) const;

	const model::templates::tradelist::TradeListTemplate* getPurchaseTemplate(int32_t id) const;

	/** Java: the HashMap; C++: a read-only map that iterates in Java's HashMap order */
	const detail::LinkedMap<int32_t, const model::templates::tradelist::TradeListTemplate*>& getTradeListTemplate() const;

	void validateBuyLists(const std::vector<const model::templates::npc::NpcTemplate*>& npcTemplates) const;
};

} // namespace aion::gameserver::dataholders
