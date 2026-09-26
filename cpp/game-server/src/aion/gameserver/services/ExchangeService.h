#pragma once

#include <cstdint>
#include <initializer_list>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/trade/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 *
 * @author ATracer
 */
class ExchangeService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::trade::Exchange>> exchanges{AION_LOCK_CLASS(ExchangeService::exchanges#stripe)}; // Java: = new ConcurrentHashMap<>()
public:
	static ExchangeService& getInstance(); // Java singleton
private:
	ExchangeService();
	~ExchangeService();
public:
	void registerExchange(model::gameobjects::player::Player& player1, model::gameobjects::player::Player& player2);
private:
	bool validateParticipants(model::gameobjects::player::Player& player1, model::gameobjects::player::Player& player2);
	runtime::Ptr<model::gameobjects::player::Player> getCurrentParter(model::gameobjects::player::Player& player);
	runtime::Ptr<model::trade::Exchange> getCurrentExchange(model::gameobjects::player::Player& player);
public:
	runtime::Ptr<model::trade::Exchange> getCurrentParnterExchange(model::gameobjects::player::Player& player);
	bool isPlayerInExchange(model::gameobjects::player::Player& player);
	void addKinah(model::gameobjects::player::Player& activePlayer, int64_t itemCount);
	void addItem(model::gameobjects::player::Player& activePlayer, int32_t itemObjId, int64_t itemCount);
	void lockExchange(model::gameobjects::player::Player& activePlayer);
	void cancelExchange(model::gameobjects::player::Player& activePlayer);
private:
	void returnItems(model::gameobjects::player::Player& player);
public:
	void confirmExchange(runtime::Ptr<model::gameobjects::player::Player> activePlayer);
private:
	void performTrade(model::gameobjects::player::Player& activePlayer, model::gameobjects::player::Player& currentPartner);
	void cleanUpExchanges(bool releaseIds, std::initializer_list<runtime::Ptr<model::gameobjects::player::Player>> players = {});
	bool removeItemsFromInventory(model::gameobjects::player::Player& player, model::trade::Exchange& exchange);
	bool validateExchange(model::gameobjects::player::Player& activePlayer, model::gameobjects::player::Player& currentPartner);
	bool validateInventorySize(model::gameobjects::player::Player& activePlayer, model::trade::Exchange& exchange);
	void putItemToInventory(model::gameobjects::player::Player& giver, model::gameobjects::player::Player& partner, model::trade::Exchange& exchange1, model::trade::Exchange& exchange2);
};

} // namespace aion::gameserver::services
