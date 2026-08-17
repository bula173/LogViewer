#include <gtest/gtest.h>
#include <cstdlib>

// Custom main() (replaces gtest_main) so we can skip static/atexit teardown
// after tests finish. Several tests construct a real QApplication; on Linux
// CI with the "offscreen" QPA platform, Qt's static teardown segfaults after
// main() would otherwise return -- by which point RUN_ALL_TESTS() has already
// fully reported every test's outcome, so terminating immediately here loses
// nothing and avoids a false-negative CI failure.
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    const int result = RUN_ALL_TESTS();
    std::_Exit(result);
}
