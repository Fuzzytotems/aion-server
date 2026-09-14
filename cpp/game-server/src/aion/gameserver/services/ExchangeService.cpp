#include "aion/gameserver/services/ExchangeService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/trade/Exchange.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("EXCHANGE_LOG");

ExchangeService::ExchangeService() = default;

ExchangeService::~ExchangeService() = default;

ExchangeService& ExchangeService::getInstance() {
	static ExchangeService instance; // Java SingletonHolder
	return instance;
}

void ExchangeService::registerExchange(model::gameobjects::player::Player& player1, model::gameobjects::player::Player& player2) {
	AION_UNPORTED();
}

bool ExchangeService::validateParticipants(model::gameobjects::player::Player& player1, model::gameobjects::player::Player& player2) {
	AION_UNPORTED();
}

runtime::Ptr<model::gameobjects::player::Player> ExchangeService::getCurrentParter(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ptr<model::trade::Exchange> ExchangeService::getCurrentExchange(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ptr<model::trade::Exchange> ExchangeService::getCurrentParnterExchange(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool ExchangeService::isPlayerInExchange(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void ExchangeService::addKinah(model::gameobjects::player::Player& activePlayer, int64_t itemCount) {
	AION_UNPORTED();
}

void ExchangeService::addItem(model::gameobjects::player::Player& activePlayer, int32_t itemObjId, int64_t itemCount) {
	AION_UNPORTED();
}

void ExchangeService::lockExchange(model::gameobjects::player::Player& activePlayer) {
	AION_UNPORTED();
}

void ExchangeService::cancelExchange(model::gameobjects::player::Player& activePlayer) {
	AION_UNPORTED();
}

void ExchangeService::returnItems(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void ExchangeService::confirmExchange(runtime::Ptr<model::gameobjects::player::Player> activePlayer) {
	AION_UNPORTED();
}

void ExchangeService::performTrade(model::gameobjects::player::Player& activePlayer, model::gameobjects::player::Player& currentPartner) {
	AION_UNPORTED();
}

void ExchangeService::cleanUpExchanges(bool releaseIds, std::initializer_list<runtime::Ptr<model::gameobjects::player::Player>> players) {
	AION_UNPORTED();
}

bool ExchangeService::removeItemsFromInventory(model::gameobjects::player::Player& player, model::trade::Exchange& exchange) {
	AION_UNPORTED();
}

bool ExchangeService::validateExchange(model::gameobjects::player::Player& activePlayer, model::gameobjects::player::Player& currentPartner) {
	AION_UNPORTED();
}

bool ExchangeService::validateInventorySize(model::gameobjects::player::Player& activePlayer, model::trade::Exchange& exchange) {
	AION_UNPORTED();
}

void ExchangeService::putItemToInventory(model::gameobjects::player::Player& giver, model::gameobjects::player::Player& partner, model::trade::Exchange& exchange1, model::trade::Exchange& exchange2) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
