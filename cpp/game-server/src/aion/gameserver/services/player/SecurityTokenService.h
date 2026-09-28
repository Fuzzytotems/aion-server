#pragma once

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/services/player/fwd.h"

namespace aion::gameserver::services::player {

/**
 * @author Artur
 */
class SecurityTokenService {
public:
	SecurityTokenService() = delete; // Java: private constructor of a static-only class
	static void generateToken(model::account::Account& account);
};

} // namespace aion::gameserver::services::player
