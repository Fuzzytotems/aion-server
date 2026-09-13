#include "aion/loginserver/network/aion/SessionKey.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/loginserver/model/Account.h"

namespace aion::loginserver::network::aion {

using namespace commons::utils;

SessionKey::SessionKey(const model::Account& acc)
	: accountId(acc.getId().value()), loginOk(Rnd::nextInt()), playOk1(Rnd::nextInt()), playOk2(Rnd::nextInt()) {}

} // namespace aion::loginserver::network::aion
