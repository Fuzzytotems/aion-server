#pragma once

// The CLIENT side of the Aion 4.8 login protocol encryption moved to the client protocol library (cpp/client-protocol, LoginClientCrypto),
// which the scenario tests and the UE client share. The tests keep this name for it.

#include "aion/clientprotocol/LoginClientCrypto.h"

namespace aion::loginserver::test {

using AionLoginClientCrypto = clientprotocol::LoginClientCrypto;

} // namespace aion::loginserver::test
