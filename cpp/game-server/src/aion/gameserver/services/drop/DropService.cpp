#include "aion/gameserver/services/drop/DropService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/item/ItemService.h"

namespace aion::gameserver::services::drop {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.drop.DropService");

DropService::DropService() = default;

DropService::~DropService() = default;

DropService& DropService::getInstance() {
	static DropService instance; // Java SingletonHolder
	return instance;
}

// Defined here (hub-headers.md §9.3): only DropService bodies use it.
class DropService::TempTradeDropPredicate final : public item::ItemService::ItemUpdatePredicate {
	AION_MAKE_REF_FRIEND
public:
	const runtime::Ref<model::gameobjects::DropNpc> dropNpc;
protected:
	explicit TempTradeDropPredicate(model::gameobjects::DropNpc& dropNpc);
public:
	static runtime::Ref<DropService::TempTradeDropPredicate> create(model::gameobjects::DropNpc& value);
	bool changeItem(model::gameobjects::Item& input) override;
protected:
	~TempTradeDropPredicate() override;
};

DropService::TempTradeDropPredicate::TempTradeDropPredicate(model::gameobjects::DropNpc& value) : dropNpc(value) {
}

runtime::Ref<DropService::TempTradeDropPredicate> DropService::TempTradeDropPredicate::create(model::gameobjects::DropNpc& value) {
	return runtime::makeRef<DropService::TempTradeDropPredicate>(value);
}

bool DropService::TempTradeDropPredicate::changeItem(model::gameobjects::Item& input) {
	AION_UNPORTED();
}

DropService::TempTradeDropPredicate::~TempTradeDropPredicate() = default;

// callback at DropService.java:55 (fieldmap key DropService@L55:44)
void DropService::scheduleFreeForAll(int32_t npcUniqueId) {
	AION_UNPORTED();
}

void DropService::unregisterDrop(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void DropService::requestDropList(runtime::Ptr<model::gameobjects::player::Player> player, int32_t npcObjectId) {
	AION_UNPORTED();
}

void DropService::closeDropList(model::gameobjects::player::Player& player, int32_t npcObjectId) {
	AION_UNPORTED();
}

bool DropService::canDistribute(model::gameobjects::player::Player& player, model::drop::DropItem& requestedItem) {
	AION_UNPORTED();
}

bool DropService::canAutoLoot(model::gameobjects::player::Player& player, model::drop::DropItem& requestedItem) {
	AION_UNPORTED();
}

void DropService::requestDropItem(model::gameobjects::player::Player& player, int32_t npcObjectId, int32_t itemIndex) {
	AION_UNPORTED();
}

void DropService::requestDropItem(model::gameobjects::player::Player& player, int32_t npcObjectId, int32_t itemIndex, bool autoLoot) {
	AION_UNPORTED();
}

void DropService::distributeEqually(model::drop::DropItem& item, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players) {
	AION_UNPORTED();
}

void DropService::resendDropList(runtime::Ptr<model::gameobjects::player::Player> player, int32_t npcObjectId, model::gameobjects::DropNpc& dropNpc, const std::unordered_set<runtime::Ptr<model::drop::DropItem>>& dropItems) {
	AION_UNPORTED();
}

void DropService::winningRollActions(model::gameobjects::player::Player& player, int32_t itemId, int32_t npcObjectId) {
	AION_UNPORTED();
}

void DropService::winningBidActions(model::gameobjects::player::Player& player, int32_t npcObjectId, int64_t highestValue) {
	AION_UNPORTED();
}

void DropService::winningNormalActions(runtime::Ptr<model::gameobjects::player::Player> player, runtime::Ptr<model::gameobjects::DropNpc> dropNpc, model::drop::DropItem& requestedItem) {
	AION_UNPORTED();
}

void DropService::see(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

// callback at DropService.java:502 (fieldmap key DropService@L502:4)
void DropService::announceDrop(model::gameobjects::player::Player& player, const model::templates::item::ItemTemplate* template_) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::drop
