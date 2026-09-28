#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/toypet/fwd.h"

namespace aion::gameserver::services::toypet {

/**
 * @author M@xx, IlBuono, xTz, Rolandas
 */
class PetService : public runtime::Immortal {
public:
	static PetService& getInstance(); // Java singleton
private:
	PetService();
public:
	void renamePet(model::gameobjects::player::Player& player, std::string_view name);
	void onPlayerLogin(model::gameobjects::player::Player& player);
	void removeObject(int32_t objectId, int32_t count, model::gameobjects::player::Player& player);
private:
	void schedule(model::gameobjects::Pet& pet, model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t count);
	void checkFeeding(model::gameobjects::Pet& pet, model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t count);
public:
	void useDoping(model::gameobjects::Pet& pet, int32_t action, int32_t itemId, int32_t slot, int32_t slot2);
private:
	bool validateSetDopeItem(model::gameobjects::Pet& pet, int32_t itemId, int32_t slot);
	bool isPetItemUseAllowed(model::gameobjects::player::Player& player, model::gameobjects::Item& item);
public:
	void activateLoot(model::gameobjects::Pet& pet, bool activate);
	void activateAutoSell(model::gameobjects::Pet& pet, bool activate);
	void sell(runtime::Ptr<model::gameobjects::Pet> pet, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items);
};

} // namespace aion::gameserver::services::toypet
