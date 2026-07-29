#include <gtest/gtest.h>
#include "utils/FileSystem.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

class FileSystemTest : public ::testing::Test
{
protected:
    fs::path dir;

    void SetUp() override
    {
        dir = fs::path(::testing::TempDir()) / "fs_test";
        fs::create_directories(dir);
    }

    void TearDown() override
    {
        fs::remove_all(dir);
    }

    std::string write_file(const std::string& name, const std::string& content)
    {
        fs::path p = dir / name;
        std::ofstream out(p, std::ios::binary);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        return p.string();
    }
};

// ==========================================================
// read_file
// ==========================================================

TEST_F(FileSystemTest, ReadsExistingFile)
{
    std::string path = write_file("hello.txt", "hello world");

    std::string content;
    ASSERT_TRUE(utils::read_file(path, content));
    EXPECT_EQ(content, "hello world");
}

TEST_F(FileSystemTest, ReadsEmptyFile)
{
    std::string path = write_file("empty.txt", "");

    std::string content = "should be cleared";
    ASSERT_TRUE(utils::read_file(path, content));
    EXPECT_TRUE(content.empty());
}

TEST_F(FileSystemTest, ReadsBinaryContentWithNullBytes)
{
    const std::string binary("\x89PNG\x00\x01\xff\r\n\x1a", 10);
    std::string path = write_file("image.png", binary);

    std::string content;
    ASSERT_TRUE(utils::read_file(path, content));
    EXPECT_EQ(content, binary);
}

TEST_F(FileSystemTest, ReturnsFalseForMissingFile)
{
    std::string content;
    EXPECT_FALSE(utils::read_file((dir / "does-not-exist.txt").string(), content));
}

TEST_F(FileSystemTest, ReturnsFalseForDirectory)
{
    std::string content;
    EXPECT_FALSE(utils::read_file(dir.string(), content));
}

// ==========================================================
// get_mime_type
// ==========================================================

TEST(MimeTypeTest, KnownExtensions)
{
    EXPECT_EQ(utils::get_mime_type("index.html"), "text/html");
    EXPECT_EQ(utils::get_mime_type("style.css"), "text/css");
    EXPECT_EQ(utils::get_mime_type("app.js"), "application/javascript");
    EXPECT_EQ(utils::get_mime_type("logo.png"), "image/png");
    EXPECT_EQ(utils::get_mime_type("photo.jpg"), "image/jpeg");
    EXPECT_EQ(utils::get_mime_type("photo.jpeg"), "image/jpeg");
    EXPECT_EQ(utils::get_mime_type("favicon.ico"), "image/x-icon");
}

TEST(MimeTypeTest, UnknownExtensionFallsBackToOctetStream)
{
    EXPECT_EQ(utils::get_mime_type("archive.zip"), "application/octet-stream");
}

TEST(MimeTypeTest, NoExtensionFallsBackToOctetStream)
{
    EXPECT_EQ(utils::get_mime_type("Makefile"), "application/octet-stream");
}

TEST(MimeTypeTest, UsesLastDotInPath)
{
    EXPECT_EQ(utils::get_mime_type("/site/v1.2/page.html"), "text/html");
    EXPECT_EQ(utils::get_mime_type("jquery.min.js"), "application/javascript");
}

TEST(MimeTypeTest, FullPathsWork)
{
    EXPECT_EQ(utils::get_mime_type("/var/www/public/css/style.css"), "text/css");
}
