#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "SpawnIds.h"

using namespace aion::gameserver::tools::regscan;

namespace {

std::vector<int32_t> ids(std::string_view text) {
	std::set<int32_t> found;
	findSpawnNpcIds(text, found);
	return {found.begin(), found.end()};
}

} // namespace

// Expected values were produced with Python's re (ASCII mode, same finditer semantics as Java's Matcher.find) and the pattern
// \bsp(?:awn)?\([^,\d]*(\d{6})(?: : (\d{6}))? of QuestSpawnAnalyzer.java.
TEST(SpawnIdsTest, MatchesQuestSpawnAnalyzerPattern) {
	using V = std::vector<int32_t>;
	EXPECT_EQ(ids("spawn(215074, x, y);"), (V{215074}));
	EXPECT_EQ(ids("sp(215074, 1f);"), (V{215074}));
	EXPECT_EQ(ids("spawn(isElyos ? 206001 : 206002, 1f);"), (V{206001, 206002}));
	EXPECT_EQ(ids("spawn(isElyos ? 206001: 206002);"), (V{206001}));
	EXPECT_EQ(ids("respawn(215074);"), (V{}));
	EXPECT_EQ(ids("_spawn(215074); x.spawn(215075);"), (V{215075}));
	EXPECT_EQ(ids("spawnNpc(215074);"), (V{}));
	EXPECT_EQ(ids("spawn (215074);"), (V{}));
	EXPECT_EQ(ids("spawn(npcId, 215074);"), (V{}));
	EXPECT_EQ(ids("spawn(21507);"), (V{}));
	EXPECT_EQ(ids("spawn(2150745);"), (V{215074}));
	EXPECT_EQ(ids("spawn(getNpc(\n  ) + 215074);"), (V{215074}));
	EXPECT_EQ(ids("spawn(Rnd.get(1, 2) == 1 ? 216001 : 216002);"), (V{}));
	EXPECT_EQ(ids("sp(sp(215074));"), (V{215074}));
	EXPECT_EQ(ids("spawn(\"x\" + 123456);"), (V{123456}));
	EXPECT_EQ(ids("spawn(isElyos ? 206001 : 206002 : 206003);"), (V{206001, 206002}));
	EXPECT_EQ(ids("// sp(700001)\n"), (V{700001}));
	EXPECT_EQ(ids("spawn(a ? 1234567 : 206002);"), (V{123456}));
	EXPECT_EQ(ids("spawn(x ? 206001 :  206002);"), (V{206001}));
	EXPECT_EQ(ids("unicod\xC3\xA9 spawn(206111)"), (V{206111}));
	EXPECT_EQ(ids("\xC3\xA9spawn(206113)"), (V{206113})); // \b is ASCII: a non-ASCII letter is no word character
	EXPECT_EQ(ids("9spawn(206112)"), (V{}));
	EXPECT_EQ(ids("spawn(206114"), (V{206114}));
	EXPECT_EQ(ids("spawn("), (V{}));
	EXPECT_EQ(ids(""), (V{}));
}
