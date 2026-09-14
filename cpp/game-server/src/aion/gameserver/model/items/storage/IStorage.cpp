#include "aion/gameserver/model/items/storage/IStorage.h"

#include "aion/gameserver/runtime/base/Unported.h"

// Packet type (docs/design/hub-headers.md §3.3): a definition returning SM_SYSTEM_MESSAGE by value needs the complete packet class, which is not an
// S0b hub (P4-06 writes SM_SYSTEM_MESSAGE.h). Not an S0b transition guard: P4-06 or P4-13 removes it once the header exists.
#if __has_include("aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h")
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::model::items::storage {

network::aion::serverpackets::SM_SYSTEM_MESSAGE IStorage::getStorageIsFullMessage() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::items::storage
#endif
