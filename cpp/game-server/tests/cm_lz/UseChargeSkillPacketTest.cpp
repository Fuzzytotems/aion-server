// CM_USE_CHARGE_SKILL (Java CM_USE_CHARGE_SKILL.java, C_FIRE_CHARGE_SKILL): the client's release of a charge skill. Play-session report of
// Zatsuko the Muse (2026-09-25 and 09-28): charge skills never fired, because this packet had no C++ class (m5e-plan.md C-04). The cases run
// the packet on the cast fixture of tests/skills/P5-02a, whose charge 1 has a 400 ms minimum and two 1,600 ms steps
// (CAST_TEST_SKILL_CHARGE_XML); the charge arithmetic of CreatureController::useChargeSkill is SkillCastPhasesTest's.

#include "../skills/P5-02a/CastTestSupport.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/network/aion/clientpackets/CM_USE_CHARGE_SKILL.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CASTSPELL_RESULT.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::test {
namespace {

using namespace std::chrono_literals;
using network::aion::clientpackets::CM_USE_CHARGE_SKILL;
using network::aion::serverpackets::SM_CASTSPELL_RESULT;
using network::test::LogCapture;

/** the decoded opcode of ClientPacketInfo.gen.inc:194 (Java AionClientPacketFactory: packets[234] = CM_USE_CHARGE_SKILL, State.IN_GAME) */
constexpr int32_t OPCODE = 234;
const char* const AUDIT_LOGGER = "AUDIT_LOG"; // AuditLogger.cpp:22

class UseChargeSkillPacketTest : public CastTest {
protected:
	/** the release: the packet has no body (CM_USE_CHARGE_SKILL.java:21-23) */
	void release() {
		cp::Driver<CM_USE_CHARGE_SKILL> packet(OPCODE);
		packet.readAndRun({}, client->get());
	}
};

TEST_F(UseChargeSkillPacketTest, WithoutACastTheReleaseDoesNothing) {
	release(); // CM_USE_CHARGE_SKILL.java:28: no casting skill - return
	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(caster.player->isCasting());
}

TEST_F(UseChargeSkillPacketTest, ANonChargeCastIsLeftAlone) {
	ConfigOverride<bool> auditLog(configs::main::LoggingConfig::LOG_AUDIT, true);
	ConfigOverride<bool> noPunishment(configs::main::PunishmentConfig::PUNISHMENT_ENABLE, false);
	LogCapture audit({AUDIT_LOGGER});
	ASSERT_TRUE(skill(TIMED_SKILL)->useSkill());
	(*client)->clearSent();

	release(); // CM_USE_CHARGE_SKILL.java:28: not a charge skill - return

	EXPECT_TRUE(caster.player->isCasting());
	EXPECT_TRUE(packetsOf<SM_CASTSPELL_RESULT>(sent()).empty());
	EXPECT_FALSE(audit.contains("tried to use charge skill")) << audit.dump();
}

TEST_F(UseChargeSkillPacketTest, AReleaseBeforeTheMinimumIsAuditedAndFiresNothing) {
	ConfigOverride<bool> auditLog(configs::main::LoggingConfig::LOG_AUDIT, true);
	ConfigOverride<bool> noPunishment(configs::main::PunishmentConfig::PUNISHMENT_ENABLE, false);
	LogCapture audit({AUDIT_LOGGER});
	ASSERT_TRUE(skill(CHARGE_START_SKILL)->useSkill());
	(*client)->clearSent();

	release(); // about 0 ms after the start: below charge 1's 400 ms minimum (CreatureController.useChargeSkill audits and returns)

	EXPECT_TRUE(packetsOf<SM_CASTSPELL_RESULT>(sent()).empty());
	EXPECT_TRUE(audit.contains("tried to use charge skill " + std::to_string(CHARGE_START_SKILL) + " after ")) << audit.dump();
}

TEST_F(UseChargeSkillPacketTest, AReleaseAfterTheMinimumFiresTheFirstChargedSkill) {
	ASSERT_TRUE(skill(CHARGE_START_SKILL)->useSkill());
	// the charge time is wall-clock time: System.currentTimeMillis() - getCastStartTime() (CM_USE_CHARGE_SKILL.java:30)
	std::this_thread::sleep_for(600ms);
	(*client)->clearSent();

	release();

	const std::vector<std::vector<uint8_t>> results = packetsOf<SM_CASTSPELL_RESULT>(sent());
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(decodeCastSpellResult(results[0]).skillId, CHARGED_SKILL_1) << "600 ms: past the 400 ms minimum, before the second 1,600 ms step";
}

} // namespace
} // namespace aion::gameserver::skillengine::test
