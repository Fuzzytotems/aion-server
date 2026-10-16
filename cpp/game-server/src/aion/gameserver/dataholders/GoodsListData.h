#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/GoodsListData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.GoodsListData.
 * <p>
 * C++: the index maps point into the bound lists, which stay after afterUnmarshal (static-data.md §2.6).
 *
 * @author ATracer
 */
class GoodsListData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/GoodsListData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::goods::GoodsList*> goodsListData;
	std::unordered_map<int32_t, const model::templates::goods::GoodsList*> goodsInListData;
	std::unordered_map<int32_t, const model::templates::goods::GoodsList*> goodsPurchaseListData;

public:
	/** @return the goods list, nullptr (Java null) if there is none */
	const model::templates::goods::GoodsList* getGoodsListById(int32_t id) const;

	const model::templates::goods::GoodsList* getGoodsInListById(int32_t id) const;

	const model::templates::goods::GoodsList* getGoodsPurchaseListById(int32_t id) const;

	/** @return goodListData.size() */
	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
