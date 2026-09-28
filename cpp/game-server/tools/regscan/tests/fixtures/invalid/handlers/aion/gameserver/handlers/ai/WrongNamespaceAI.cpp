#include "aion/gameserver/handlers/HandlerRegistry.h"

namespace aion::gameserver::handlers::instance {

class WrongNamespaceAI final {};

} // namespace aion::gameserver::handlers::instance

namespace aion::gameserver::handlers::instance {
AION_AI(WrongNamespaceAI, "wrong_namespace");
}
