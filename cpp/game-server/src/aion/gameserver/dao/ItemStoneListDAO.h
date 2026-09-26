#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/commons/database/Connection.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/ItemStone_ItemStoneType.h"
#include "aion/gameserver/model/items/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author ATracer, Wakizashi
 */
class ItemStoneListDAO {
public:
	static void load(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items);
	static void save(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items);
	static void storeManaStones(const std::unordered_set<runtime::Ptr<model::items::ManaStone>>& manaStones);
	static void storeGodStones(model::items::GodStone& godStones);
	static void storeFusionStone(const std::unordered_set<runtime::Ptr<model::items::ManaStone>>& manaStones);
	static void storeIdianStones(model::items::IdianStone& idianStone);
private:
	static void store(const std::unordered_set<runtime::Ptr<model::items::ItemStone>>& stones, model::items::ItemStone_ItemStoneType ist);
	static void addItemStones(commons::database::Connection& con, const std::vector<runtime::Ptr<model::items::ItemStone>>& itemStones,
		model::items::ItemStone_ItemStoneType ist);
	static void updateItemStones(commons::database::Connection& con, const std::vector<runtime::Ptr<model::items::ItemStone>>& itemStones,
		model::items::ItemStone_ItemStoneType ist);
	static void deleteItemStones(commons::database::Connection& con, const std::vector<runtime::Ptr<model::items::ItemStone>>& itemStones,
		model::items::ItemStone_ItemStoneType ist);
	static void deleteItemStone(commons::database::Connection& con, int32_t uid, int32_t slot, int32_t category);
public:
	/** Saves stones of player */
	static void save(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::dao
