#pragma once

#include "aion/commons/utils/Exception.h"

namespace aion::commons::network {

/**
 * Thrown when a socket operation fails at the network level, e.g. an outbound connection is refused or times out (Java: java.net.SocketException
 * and its subclasses like ConnectException). The game server distinguishes it from other IOExceptions to choose the reconnect delay.
 */
class SocketException : public utils::IOException {
public:
	using utils::IOException::IOException;
};

} // namespace aion::commons::network
