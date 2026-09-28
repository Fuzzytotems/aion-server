#pragma once

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "Lexer.h"
#include "Registry.h"
#include "SourceScanner.h"

namespace aion::gameserver::tools::regscan::test {

/** Scans an in-memory handler file; the display path equals the relative path. */
inline FileScan scanHandler(std::string_view relPath, std::string_view code) {
	return scanCppFile(relPath, relPath, code, FileRole::HANDLER);
}

inline FileScan scanClientPacket(std::string_view relPath, std::string_view code) {
	return scanCppFile(relPath, relPath, code, FileRole::CLIENT_PACKET);
}

inline std::string describe(const std::vector<Diagnostic>& errors) {
	std::string text;
	for (const Diagnostic& d : errors)
		text += d.format() + "\n";
	return text.empty() ? "(no errors)" : text;
}

/** @return true if there is an error on the given line (0: any line) whose message contains the text */
inline bool hasError(const std::vector<Diagnostic>& errors, uint32_t line, std::string_view text) {
	for (const Diagnostic& d : errors) {
		if ((line == 0 || d.line == line) && d.message.find(text) != std::string::npos)
			return true;
	}
	return false;
}

#define EXPECT_ERROR(errors, line, text) EXPECT_TRUE(::aion::gameserver::tools::regscan::test::hasError(errors, line, text)) << ::aion::gameserver::tools::regscan::test::describe(errors)
#define EXPECT_NO_ERRORS(errors) EXPECT_TRUE((errors).empty()) << ::aion::gameserver::tools::regscan::test::describe(errors)

inline SourceFile source(std::string relPath, std::string content) {
	return SourceFile{relPath, relPath, std::move(content)};
}

} // namespace aion::gameserver::tools::regscan::test
