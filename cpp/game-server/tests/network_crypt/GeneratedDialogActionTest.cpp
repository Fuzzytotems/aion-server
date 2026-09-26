// Compiles and checks the generated model/DialogAction.h and DialogAction.gen.cpp (cpp/tools/gen/dialogaction.py).
// The C and Windows headers come first so that any constant whose name collides with one of their macros fails to compile here.
#include <cerrno>
#include <cfloat>
#include <climits>
#include <clocale>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "aion/commons/utils/WindowsMacroGuard.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>

#include "aion/gameserver/model/DialogAction.h"

#include "aion/gameserver/model/DialogAction.gen.cpp"

namespace aion::gameserver::model {
namespace {

TEST(GeneratedDialogActionTest, ConstantsHaveTheJavaValues) {
	static_assert(DialogAction::USE_OBJECT == -1);
	static_assert(DialogAction::NULL_ == 1); // Java: NULL
	static_assert(DialogAction::QUEST_SELECT == 31);
	static_assert(DialogAction::FINISH_DIALOG == 1008);
	static_assert(DialogAction::SELECT1 == 1011);
	static_assert(DialogAction::SET_SUCCEED == 10255);
	static_assert(DialogAction::SETPRO_NEXT == 20003);
	static_assert(DialogAction::OPEN_WEB_SHOP == 100001);
	SUCCEED();
}

TEST(GeneratedDialogActionTest, NameOfReturnsTheJavaNameOrNothing) {
	EXPECT_EQ(DialogAction::nameOf(-1), "USE_OBJECT");
	EXPECT_EQ(DialogAction::nameOf(DialogAction::NULL_), "NULL");
	EXPECT_EQ(DialogAction::nameOf(31), "QUEST_SELECT");
	EXPECT_EQ(DialogAction::nameOf(100001), "OPEN_WEB_SHOP");
	EXPECT_EQ(DialogAction::nameOf(0), std::nullopt); // "0 doesn't exist"
	EXPECT_EQ(DialogAction::nameOf(102), std::nullopt);
	EXPECT_EQ(DialogAction::nameOf(-2), std::nullopt);
	EXPECT_EQ(DialogAction::nameOf(100002), std::nullopt);
	EXPECT_EQ(DialogAction::nameOf(INT32_MIN), std::nullopt);
	EXPECT_EQ(DialogAction::nameOf(INT32_MAX), std::nullopt);
}

TEST(GeneratedDialogActionTest, EntriesAreSortedUniqueAndComplete) {
	const auto entries = DialogAction::entries();
	EXPECT_EQ(entries.size(), 6205u);
	EXPECT_TRUE(std::ranges::is_sorted(entries, std::ranges::less{}, &DialogAction::Entry::id));
	std::set<std::string_view> names;
	for (size_t i = 0; i < entries.size(); i++) {
		if (i > 0)
			ASSERT_LT(entries[i - 1].id, entries[i].id);
		names.insert(entries[i].name);
		ASSERT_EQ(DialogAction::nameOf(entries[i].id), entries[i].name);
	}
	EXPECT_EQ(names.size(), entries.size());
}

TEST(GeneratedDialogActionTest, UsingDirectiveWorksLikeJavasStaticImport) {
	using namespace DialogAction;
	EXPECT_EQ(QUEST_SELECT, 31);
	EXPECT_EQ(SELECT1, 1011);
}

} // namespace
} // namespace aion::gameserver::model
