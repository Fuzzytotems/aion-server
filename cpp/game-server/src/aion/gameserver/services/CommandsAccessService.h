#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author ViAl, Neon
 */
class CommandsAccessService {
private:
	static inline runtime::Field<runtime::Ref<runtime::RcHashMap<int32_t, runtime::Ref<runtime::RcHashSet<std::string>>>>> commandAccesses{}; // Java: = Collections.emptyMap(); null until loadAccesses() (the port creates the empty map)
	CommandsAccessService() = delete;
public:
	static void loadAccesses();
	static void giveTemporaryAccess(model::gameobjects::player::Player& admin, int32_t playerId, std::string_view command);
	static void giveAccess(model::gameobjects::player::Player& admin, int32_t playerId, std::string_view command);
private:
	static void giveAccess(model::gameobjects::player::Player& admin, int32_t playerId, std::string_view command, bool isTemporary);
public:
	static void removeAccess(model::gameobjects::player::Player& admin, int32_t playerId, std::string_view command);
	static bool removeAllAccesses(int32_t playerId);
	static bool hasAccess(int32_t playerId, std::string_view command);
};

} // namespace aion::gameserver::services
