#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/drop/DropGroup.xml.h"

#include "aion/gameserver/model/drop/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::drop {

/**
 * Java com.aionemu.gameserver.model.drop.DropGroup.
 * <p>
 * C++: the result set is the caller's shim (hub-headers.md §7.1, `DropRegistrationService.addDropItems`); `groupMembers` empty stands for Java's
 * null or empty collection (both take the same branch).
 *
 * @author MrPoke
 */
class DropGroup : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/drop/DropGroup.xml.inc"
public:
	int32_t tryAddDropItems(runtime::RcHashSet<runtime::Ref<DropItem>>& result, int32_t index, DropModifiers& dropModifiers,
		const std::vector<runtime::Ptr<gameobjects::player::Player>>& groupMembers) const;

private:
	int32_t addDropItem(int32_t index, runtime::RcHashSet<runtime::Ref<DropItem>>& result, const Drop& drop,
		const std::vector<runtime::Ptr<gameobjects::player::Player>>& groupMembers) const;
};

} // namespace aion::gameserver::model::drop
