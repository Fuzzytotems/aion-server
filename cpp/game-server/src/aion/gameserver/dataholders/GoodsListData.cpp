#include "aion/gameserver/dataholders/GoodsListData.h"

namespace aion::gameserver::dataholders {

namespace {

using model::templates::goods::GoodsList;
using GoodsMap = std::unordered_map<int32_t, const GoodsList*>;

const GoodsList* find(const GoodsMap& map, int32_t id) {
	auto it = map.find(id);
	return it != map.end() ? it->second : nullptr;
}

} // namespace

void GoodsListData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const GoodsList& it : list)
		goodsListData.insert_or_assign(it.getId(), &it);
	for (const GoodsList& it : inList)
		goodsInListData.insert_or_assign(it.getId(), &it);
	for (const GoodsList& it : purchaseList)
		goodsPurchaseListData.insert_or_assign(it.getId(), &it);
	// Java: list = inList = purchaseList = null (the C++ indexes point into the storage, which stays)
}

const GoodsList* GoodsListData::getGoodsListById(int32_t id) const {
	return find(goodsListData, id);
}

const GoodsList* GoodsListData::getGoodsInListById(int32_t id) const {
	return find(goodsInListData, id);
}

const GoodsList* GoodsListData::getGoodsPurchaseListById(int32_t id) const {
	return find(goodsPurchaseListData, id);
}

int32_t GoodsListData::size() const {
	return static_cast<int32_t>(goodsListData.size() + goodsInListData.size() + goodsPurchaseListData.size());
}

} // namespace aion::gameserver::dataholders
