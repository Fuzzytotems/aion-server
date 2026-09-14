#include "aion/gameserver/model/items/storage/IStorage.h"

#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::items::storage {

network::aion::serverpackets::SM_SYSTEM_MESSAGE IStorage::getStorageIsFullMessage() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::items::storage
