#include "aion/gameserver/dataholders/TradeListData.h"

#include <algorithm>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"

namespace aion::gameserver::dataholders {

using model::templates::npc::NpcTemplate;
using model::templates::tradelist::TradeListTemplate;

namespace {

const TradeListTemplate* find(const std::unordered_map<int32_t, const TradeListTemplate*>& map, int32_t id) {
	auto it = map.find(id);
	return it != map.end() ? it->second : nullptr;
}

/** Java List<Integer>.toString(): "[1, 2, 3]" */
std::string listToString(const std::vector<int32_t>& ids) {
	std::string text = "[";
	for (size_t i = 0; i < ids.size(); ++i) {
		if (i > 0)
			text += ", ";
		text += std::to_string(ids[i]);
	}
	return text + "]";
}

} // namespace

void TradeListData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	detail::JavaHashMapOrder<int32_t, const TradeListTemplate*> order;
	for (const TradeListTemplate& npc : tlist)
		order.put(npc.getNpcId(), &npc, detail::javaHashCode(npc.getNpcId()));
	npctlistData = detail::toLinkedMap<decltype(npctlistData)>(order);
	for (const TradeListTemplate& npc : tInlist)
		npcTradeInlistData.insert_or_assign(npc.getNpcId(), &npc);
	for (const TradeListTemplate& npc : plist)
		npcPurchaseTemplateData.insert_or_assign(npc.getNpcId(), &npc);
	// Java: tlist = tInlist = plist = null (the C++ indexes point into the storage, which stays)
}

int32_t TradeListData::size() const {
	return static_cast<int32_t>(npctlistData.size());
}

const TradeListTemplate* TradeListData::getTradeListTemplate(int32_t id) const {
	auto* npc = npctlistData.get(id);
	return npc != nullptr ? *npc : nullptr;
}

const TradeListTemplate* TradeListData::getTradeInListTemplate(int32_t id) const {
	return find(npcTradeInlistData, id);
}

const TradeListTemplate* TradeListData::getPurchaseTemplate(int32_t id) const {
	return find(npcPurchaseTemplateData, id);
}

const detail::LinkedMap<int32_t, const TradeListTemplate*>& TradeListData::getTradeListTemplate() const {
	return npctlistData;
}

void TradeListData::validateBuyLists(const std::vector<const NpcTemplate*>& npcTemplates) const {
	const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.TradeListData");
	std::vector<int32_t> missingNpcIds;
	for (const NpcTemplate* npc : npcTemplates) {
		if (npc->supportsAction(model::DialogAction::BUY) && getTradeListTemplate(npc->getTemplateId()) == nullptr)
			missingNpcIds.push_back(npc->getTemplateId());
	}
	std::sort(missingNpcIds.begin(), missingNpcIds.end());
	if (!missingNpcIds.empty())
		log.warn("Missing trade lists for these npcs: " + listToString(missingNpcIds));
	missingNpcIds.clear();
	for (const NpcTemplate* npc : npcTemplates) {
		if (npc->supportsAction(model::DialogAction::TRADE_IN) && getTradeInListTemplate(npc->getTemplateId()) == nullptr)
			missingNpcIds.push_back(npc->getTemplateId());
	}
	std::sort(missingNpcIds.begin(), missingNpcIds.end());
	if (!missingNpcIds.empty())
		log.warn("Missing trade-in lists for these npcs: " + listToString(missingNpcIds));
}

} // namespace aion::gameserver::dataholders
