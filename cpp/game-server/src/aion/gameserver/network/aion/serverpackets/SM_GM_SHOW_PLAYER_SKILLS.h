#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/skill/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author Yeats
 */
class SM_GM_SHOW_PLAYER_SKILLS : public AionServerPacket {
public:
	static constexpr int32_t STATIC_BODY_SIZE = 2;

private:
	std::vector<runtime::Ref<model::skill::PlayerSkillEntry>> skillList{};

public:
	explicit SM_GM_SHOW_PLAYER_SKILLS(const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skillList);
	~SM_GM_SHOW_PLAYER_SKILLS() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
