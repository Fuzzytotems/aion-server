#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/skill/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author MrPoke, ATracer, Neon
 */
class SM_SKILL_LIST : public AionServerPacket {
public:
	static constexpr int32_t STATIC_BODY_SIZE = 7;
private:
	std::vector<runtime::Ref<model::skill::PlayerSkillEntry>> skillList{};
	int32_t messageId{};
	std::string skillNameL10n{};
	std::string skillLvl{};
public:
	bool silentUpdate{}; // Java: = false
	explicit SM_SKILL_LIST(const std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>>& skillList);
	SM_SKILL_LIST(model::skill::PlayerSkillEntry& skill, int32_t messageId);
	~SM_SKILL_LIST() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
