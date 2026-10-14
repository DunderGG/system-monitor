#include <gtest/gtest.h>
#include <QImageReader>
#include <QPixmap>

// Qt's Windows platform plugin builds its split, hand, and drag cursors from
// PNG resources. Without qtbase's "png" feature in vcpkg.json they load as
// null pixmaps, and showing one (a header's column-resize cursor, for
// example) fails an assertion in debug builds. See DC-86.

TEST(QtConfiguration, ImageFormats_PngReaderAvailable)
{
    EXPECT_TRUE(QImageReader::supportedImageFormats().contains("png"));
}

TEST(QtConfiguration, WindowsCursorResources_Load)
{
    EXPECT_FALSE(QPixmap(":/qt-project.org/windows/cursors/images/splithcursor_32.png").isNull());
    EXPECT_FALSE(QPixmap(":/qt-project.org/windows/cursors/images/dragmovecursor_32.png").isNull());
}
