#include "aion/gameserver/network/aion/serverpackets/SM_GM_SHOW_LEGION_INFO.h"

#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GM_SHOW_LEGION_INFO::SM_GM_SHOW_LEGION_INFO(model::team::legion::Legion& legion) : SM_LEGION_INFO(opcodeOf<SM_GM_SHOW_LEGION_INFO>, legion) {
}
} // namespace aion::gameserver::network::aion::serverpackets
