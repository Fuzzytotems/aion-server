#include "aion/commons/configuration/transformers/FileTransformer.h"

#include <gtest/gtest.h>

using namespace aion::commons::configuration::transformers;

TEST(FileTransformerTest, PathIsKeptAsWritten) {
	EXPECT_EQ(transform<std::filesystem::path>("./data/handlers/ai"), std::filesystem::path("./data/handlers/ai"));
	EXPECT_EQ(transform<std::filesystem::path>(""), std::filesystem::path());
	EXPECT_EQ(typeName<std::filesystem::path>(), "File");
}

TEST(FileTransformerTest, Utf8IsDecodedIndependentOfCodePage) {
	std::filesystem::path path = transform<std::filesystem::path>("d\xC3\xA4t\xE4\xB8\xAD/x");
	// EXPECT_TRUE: the prebuilt GoogleTest cannot print std::u8string
	EXPECT_TRUE(path.u8string() == u8"dät中/x");
	EXPECT_TRUE(path.filename().u8string() == u8"x");
	EXPECT_EQ(path.wstring(), L"dät中/x");
}
