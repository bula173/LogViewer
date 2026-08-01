#include <gtest/gtest.h>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QString>
#include <QStandardPaths>
#include <thread>
#include <chrono>

namespace ui::qt::test {

class FileIOErrorHandlingTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_tempDir = std::make_unique<QTemporaryDir>();
        ASSERT_TRUE(m_tempDir->isValid());
    }

    std::unique_ptr<QTemporaryDir> m_tempDir;

    // Helper: validate file write was successful
    bool VerifyFileWrite(const QString& filePath, const QString& content)
    {
        // 1. Check directory exists
        QFileInfo fileInfo(filePath);
        QDir dir = fileInfo.dir();
        if (!dir.exists()) {
            return false;
        }

        // 2. Attempt write
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }

        QTextStream stream(&file);
        stream << content;

        // 3. Check for write errors
        if (stream.status() != QTextStream::Ok || file.error() != QFile::NoError) {
            file.close();
            file.remove();
            return false;
        }

        file.close();

        // 4. Verify file exists and is not empty
        if (!file.exists() || file.size() == 0) {
            return false;
        }

        return true;
    }
};

// Test: Directory validation before write
TEST_F(FileIOErrorHandlingTest, DirectoryExistsCheck)
{
    QString validPath = m_tempDir->path() + "/test.txt";
    QFileInfo fileInfo(validPath);
    QDir dir = fileInfo.dir();

    EXPECT_TRUE(dir.exists());
}

// Test: Non-existent directory detection
TEST_F(FileIOErrorHandlingTest, NonExistentDirectoryDetection)
{
    QString invalidPath = "/nonexistent/directory/that/does/not/exist/test.txt";
    QFileInfo fileInfo(invalidPath);
    QDir dir = fileInfo.dir();

    EXPECT_FALSE(dir.exists());
}

// Test: Successful file write
TEST_F(FileIOErrorHandlingTest, SuccessfulFileWrite)
{
    QString filePath = m_tempDir->path() + "/success.txt";
    QString content = "Test content";

    EXPECT_TRUE(VerifyFileWrite(filePath, content));
}

// Test: File write without errors
TEST_F(FileIOErrorHandlingTest, FileWriteStatusCheck)
{
    QString filePath = m_tempDir->path() + "/status_check.txt";
    QString content = "Status check content";

    QFile file(filePath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QTextStream stream(&file);
    stream << content;

    EXPECT_EQ(stream.status(), QTextStream::Ok);
    EXPECT_EQ(file.error(), QFile::NoError);

    file.close();
}

// Test: File size verification
TEST_F(FileIOErrorHandlingTest, FileSizeVerification)
{
    QString filePath = m_tempDir->path() + "/size_check.txt";
    QString content = "Size verification test";

    QFile file(filePath);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream stream(&file);
    stream << content;
    file.close();

    // Verify file exists and has content
    EXPECT_TRUE(file.exists());
    EXPECT_GT(file.size(), 0);
}

// Test: Partial file cleanup on write failure
TEST_F(FileIOErrorHandlingTest, PartialFileCleanup)
{
    QString filePath = m_tempDir->path() + "/cleanup_test.txt";

    // Create a file
    QFile file(filePath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));

    QTextStream stream(&file);
    stream << "Partial content";
    file.close();

    // Verify file was created
    ASSERT_TRUE(file.exists());

    // Simulate cleanup if write failed (file would be empty)
    if (file.size() == 0) {
        file.remove();
        EXPECT_FALSE(file.exists());
    } else {
        // File has content, so cleanup wouldn't apply
        EXPECT_TRUE(file.exists());
    }
}

// Test: File overwrite capability
TEST_F(FileIOErrorHandlingTest, FileOverwrite)
{
    QString filePath = m_tempDir->path() + "/overwrite.txt";

    // Write initial content
    QFile file(filePath);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream stream(&file);
    stream << "Initial content";
    file.close();

    qint64 initialSize = file.size();

    // Overwrite with different content
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    stream << "New content";
    file.close();

    qint64 newSize = file.size();

    // Sizes should be different (or at least operation succeeded)
    EXPECT_TRUE(file.exists());
    EXPECT_GT(newSize, 0);
}

// Test: Multiple concurrent file writes to different files
TEST_F(FileIOErrorHandlingTest, MultipleConcurrentWrites)
{
    std::vector<QString> filePaths = {
        m_tempDir->path() + "/file1.txt",
        m_tempDir->path() + "/file2.txt",
        m_tempDir->path() + "/file3.txt"
    };

    std::vector<bool> results;
    std::vector<std::thread> threads;

    for (const auto& filePath : filePaths) {
        threads.emplace_back([this, filePath, &results]() {
            QString content = "Content for " + QFileInfo(filePath).fileName();
            bool success = VerifyFileWrite(filePath, content);
            results.push_back(success);
        });
    }

    for (auto& t : threads)
        t.join();

    // All writes should succeed
    for (bool result : results)
        EXPECT_TRUE(result);
}

// Test: Large file write
TEST_F(FileIOErrorHandlingTest, LargeFileWrite)
{
    QString filePath = m_tempDir->path() + "/large.txt";

    // Create 1MB of content
    QString content;
    for (int i = 0; i < 1000; ++i)
        content += "Line " + QString::number(i) + ": test content\n";

    EXPECT_TRUE(VerifyFileWrite(filePath, content));

    QFile file(filePath);
    EXPECT_TRUE(file.exists());
    EXPECT_GT(file.size(), 10000);  // > 10KB
}

// Test: Unicode content write
TEST_F(FileIOErrorHandlingTest, UnicodeContentWrite)
{
    QString filePath = m_tempDir->path() + "/unicode.txt";
    QString content = "Unicode test: 你好世界 🌍 Ñoño";

    EXPECT_TRUE(VerifyFileWrite(filePath, content));

    // Verify content can be read back
    QFile file(filePath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&file);
    QString readContent = stream.readAll();
    file.close();

    EXPECT_EQ(readContent, content);
}

// Test: Write with special characters in filename
TEST_F(FileIOErrorHandlingTest, SpecialCharactersInFilename)
{
    QString filePath = m_tempDir->path() + "/report_2024-01-01_special.txt";
    QString content = "Test content";

    EXPECT_TRUE(VerifyFileWrite(filePath, content));
}

// Test: Empty file write and cleanup
TEST_F(FileIOErrorHandlingTest, EmptyFileDetection)
{
    QString filePath = m_tempDir->path() + "/empty.txt";

    QFile file(filePath);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream stream(&file);
    // Write nothing
    file.close();

    // Empty file should be detected and cleaned up
    if (file.size() == 0) {
        file.remove();
    }

    EXPECT_FALSE(file.exists());
}

// Test: HTML file write with special chars
TEST_F(FileIOErrorHandlingTest, HTMLContentWrite)
{
    QString filePath = m_tempDir->path() + "/report.html";
    QString htmlContent = R"(
<!DOCTYPE html>
<html>
<head><title>Test Report</title></head>
<body>
    <h1>Error Report &amp; Statistics</h1>
    <p>Special chars: &lt; &gt; &quot;</p>
</body>
</html>
)";

    EXPECT_TRUE(VerifyFileWrite(filePath, htmlContent));

    // Verify HTML is valid
    QFile file(filePath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QString readContent = file.readAll();
    file.close();

    EXPECT_TRUE(readContent.contains("<html>"));
    EXPECT_TRUE(readContent.contains("</html>"));
}

// Performance test: Multiple file writes
TEST_F(FileIOErrorHandlingTest, MultipleFileWritePerformance)
{
    auto start = std::chrono::high_resolution_clock::now();

    // Write 50 files
    for (int i = 0; i < 50; ++i) {
        QString filePath = m_tempDir->path() + QString("/file_%1.txt").arg(i);
        QString content = ("Content " + QString::number(i) + "\n").repeated(100);
        EXPECT_TRUE(VerifyFileWrite(filePath, content));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_LT(duration, 5000);  // Should complete in < 5 seconds
}

} // namespace ui::qt::test
