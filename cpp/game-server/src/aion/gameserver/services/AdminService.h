#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder. Java's
 * public constructor is private here (it reads the restriction file).
 *
 * @author KID
 */
class AdminService : public runtime::Immortal {
private:
	runtime::ArrayList<int32_t> list{AION_LOCK_CLASS(AdminService::list)};
public:
	static AdminService& getInstance(); // Java singleton
private:
	AdminService();
	~AdminService();
public:
	void reload();
	bool canOperate(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::player::Player> target, model::gameobjects::Item& item, std::string_view type);
	bool canOperate(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::player::Player> target, int32_t itemId, std::string_view type);
};

} // namespace aion::gameserver::services
