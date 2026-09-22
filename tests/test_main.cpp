#include <cstring>

#include <QApplication>
#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    for (int i = 1; i < argc; ++i) {
        if (std::strstr(argv[i], "--gtest_list_tests") != nullptr) {
            return RUN_ALL_TESTS();
        }
    }

    QApplication application(argc, argv);
    return RUN_ALL_TESTS();
}

