#include <gtest/gtest.h>
#include <QApplication>

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    // Listing the tests needs no QApplication, whose platform plugin can fail
    // to load (DC-21). InitGoogleTest() removes the flags it reads from argv,
    // so the flag is read from GoogleTest.
    if (GTEST_FLAG_GET(list_tests)) {
        return RUN_ALL_TESTS();
    }

    QApplication application(argc, argv);
    return RUN_ALL_TESTS();
}
