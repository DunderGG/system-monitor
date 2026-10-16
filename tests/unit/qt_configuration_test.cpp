#include <gtest/gtest.h>
#include <QApplication>
#include <QImageReader>
#include <QPixmap>
#include <QStringList>
#include <QStyle>
#include <QStyleFactory>

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

// Qt's Windows 11 and Windows Vista styles are a plugin that Qt loads at run
// time from a "styles" directory, which the build copies next to every
// executable. Without it Qt falls back to its built-in classic "Windows"
// style, with bevelled controls. See DC-104.

TEST(QtConfiguration, StylePlugin_ModernWindowsStylesAvailable)
{
    const QStringList keys = QStyleFactory::keys();
    EXPECT_TRUE(keys.contains("windows11", Qt::CaseInsensitive)) << keys.join(", ").toStdString();
    EXPECT_TRUE(keys.contains("windowsvista", Qt::CaseInsensitive)) << keys.join(", ").toStdString();
}

TEST(QtConfiguration, ApplicationStyle_IsAModernWindowsStyle)
{
    // Qt picks windows11 on Windows 11 and windowsvista on Windows 10.
    const QString name = QApplication::style()->name();
    EXPECT_TRUE(name.compare("windows11", Qt::CaseInsensitive) == 0 ||
                name.compare("windowsvista", Qt::CaseInsensitive) == 0)
        << name.toStdString();
}
