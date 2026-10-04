// The "Select File Type" prompt (shown for unknown extensions) must offer
// every built-in format, and each offered extension must map to a parser.
#include <gtest/gtest.h>

#include "qt/MainWindow.hpp"
#include "ParserFactory.hpp"

#include <algorithm>

namespace ui::qt::test
{

TEST(MainWindowFileTypeChoicesTest, OffersJson)
{
    const auto choices = MainWindow::FileTypeChoices();
    const bool hasJson = std::any_of(choices.begin(), choices.end(),
        [](const MainWindow::FileTypeChoice& c) { return c.ext == ".json"; });
    EXPECT_TRUE(hasJson);
}

TEST(MainWindowFileTypeChoicesTest, EveryChoiceHasAParser)
{
    for (const auto& c : MainWindow::FileTypeChoices())
    {
        const std::string ext = c.ext.toStdString();
        // .asc and .evl are built directly by MainWindow::CreateParserFor().
        const bool handled = ext == ".asc" || ext == ".evl"
            || parser::ParserFactory::IsRegistered(ext);
        EXPECT_TRUE(handled) << ext;
    }
}

} // namespace ui::qt::test
