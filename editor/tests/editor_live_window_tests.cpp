#include <catch2/catch_test_macros.hpp>

#include "editor_timeline.h"
#include "editor_viewport.h"
#include "editor_window.h"
#include "sample_package.h"

#include "document/animation_strings.h"
#include "document/document.h"
#include "document/outline.h"
#include "document/stage_bounds.h"
#include "document/frame_edit.h"
#include "document/place_image.h"
#include "document/tags.h"
#include "formats/ifs_archive.h"

#include <QAction>
#include <QImage>
#include <QPixmap>
#include <QtGlobal>
#include <QApplication>
#include <QByteArray>
#include <QColorDialog>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QEvent>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QSettings>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "window_test_support.h"

using namespace WindowTest;

TEST_CASE("A stage drag previews through the host before it is committed") {
    const QString game = qEnvironmentVariable("R573_IIDX_DIR");
    if (game.isEmpty()) SKIP("R573_IIDX_DIR not set");
    const QString title = game + "/data/graphic/1/title.ifs";
    QSettings().setValue("game/directory", game);
    Editor::Window window;
    QSettings().remove("game/directory");
    window.resize(1600, 900);
    window.show();
    Script opening({});
    window.OpenDocument(title);
    REQUIRE(opening.Problems().isEmpty());

    const std::optional<uint16_t> widest = WidestTitleDepth(title);
    REQUIRE(widest.has_value());
    if (!widest) return;

    auto* timeline = window.findChild<Editor::Timeline*>();
    auto* viewport = window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    QAction* undo = ShortcutAction(window, QKeySequence(QKeySequence::Undo));
    REQUIRE(undo != nullptr);
    emit timeline->FrameChosen(kPreviewFrame);
    emit timeline->DepthChosen(*widest);
    QApplication::processEvents();
    const QImage before = viewport->grab().toImage();

    emit viewport->Dragged(*widest, 300, 150, false);
    QApplication::processEvents();
    QApplication::processEvents();
    const QImage during = viewport->grab().toImage();
    CHECK(during != before);
    CHECK_FALSE(undo->isEnabled());

    emit viewport->Dragged(*widest, 0, 0, false);
    QApplication::processEvents();
    QApplication::processEvents();
    CHECK(viewport->grab().toImage() == before);
    CHECK(opening.Problems().isEmpty());
}

TEST_CASE("Hiding a depth in the view takes it out of the rendered frame until it is shown") {
    const QString game = qEnvironmentVariable("R573_IIDX_DIR");
    if (game.isEmpty()) SKIP("R573_IIDX_DIR not set");
    const QString title = game + "/data/graphic/1/title.ifs";
    const std::optional<uint16_t> widest = WidestTitleDepth(title);
    REQUIRE(widest.has_value());
    if (!widest) return;
    QSettings().setValue("game/directory", game);
    Editor::Window window;
    QSettings().remove("game/directory");
    window.resize(1600, 900);
    window.show();
    Script opening({});
    window.OpenDocument(title);
    REQUIRE(opening.Problems().isEmpty());
    auto* timeline = window.findChild<Editor::Timeline*>();
    auto* viewport = window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    const auto grab = [&] {
        emit timeline->DepthChosen(kNoDepth);
        QApplication::processEvents();
        return viewport->grab().toImage();
    };
    emit timeline->FrameChosen(kPreviewFrame);
    const QImage before = grab();
    emit timeline->DepthChosen(*widest);
    const QString depth = QString::number(*widest);
    {
        Script hidden({Choose("Hide depth " + depth + " in the view")});
        emit timeline->MenuRequested(QPoint(4, 4), kPreviewFrame, QString());
        REQUIRE(Settle([&hidden] { return hidden.Finished(); }));
        CHECK(hidden.Problems().isEmpty());
    }
    CHECK(grab() != before);
    emit timeline->DepthChosen(*widest);
    {
        Script shown({Choose("Show every hidden depth")});
        emit timeline->MenuRequested(QPoint(4, 4), kPreviewFrame, QString());
        REQUIRE(Settle([&shown] { return shown.Finished(); }));
        CHECK(shown.Problems().isEmpty());
    }
    CHECK(grab() == before);
    CHECK(opening.Problems().isEmpty());
}

TEST_CASE("Playback stays inside the work area") {
    const QString game = qEnvironmentVariable("R573_IIDX_DIR");
    if (game.isEmpty()) SKIP("R573_IIDX_DIR not set");
    QSettings().setValue("game/directory", game);
    Editor::Window window;
    QSettings().remove("game/directory");
    window.resize(1600, 900);
    window.show();
    Script opening({});
    window.OpenDocument(game + "/data/graphic/1/title.ifs");
    REQUIRE(opening.Problems().isEmpty());
    auto* timeline = window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    const auto at = [&](uint32_t frame) {
        emit timeline->FrameChosen(frame);
        QApplication::processEvents();
        return timeline->grab().toImage();
    };
    QAction* start = ShortcutAction(window, QKeySequence(Qt::Key_B));
    QAction* end = ShortcutAction(window, QKeySequence(Qt::Key_N));
    QAction* play = ShortcutAction(window, QKeySequence(Qt::Key_Space));
    REQUIRE(start != nullptr);
    REQUIRE(end != nullptr);
    REQUIRE(play != nullptr);
    at(kPreviewFrame + 2);
    end->trigger();
    at(kPreviewFrame);
    start->trigger();
    const std::vector<QImage> inside{at(kPreviewFrame), at(kPreviewFrame + 1),
                                     at(kPreviewFrame + 2)};
    play->trigger();
    QElapsedTimer played;
    played.start();
    while (played.elapsed() < kPlayForMs)
        QApplication::processEvents();
    play->trigger();
    const QImage stopped = timeline->grab().toImage();
    CHECK(std::ranges::find(inside, stopped) != inside.end());
    CHECK(opening.Problems().isEmpty());
}

TEST_CASE("A saved frame is the stage size, opaque, and leaves the viewport as it was") {
    const QString game = qEnvironmentVariable("R573_IIDX_DIR");
    if (game.isEmpty()) SKIP("R573_IIDX_DIR not set");
    QSettings().setValue("game/directory", game);
    Editor::Window window;
    QSettings().remove("game/directory");
    window.resize(1600, 900);
    window.show();
    Script opening({});
    window.OpenDocument(game + "/data/graphic/1/title.ifs");
    REQUIRE(opening.Problems().isEmpty());
    auto* timeline = window.findChild<Editor::Timeline*>();
    auto* viewport = window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    emit timeline->FrameChosen(kPreviewFrame);
    QApplication::processEvents();
    const QImage before = viewport->grab().toImage();

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("frame.png");
    QAction* save = ShortcutAction(window, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
    REQUIRE(save != nullptr);
    {
        Script saving({PickFile(path)});
        save->trigger();
        REQUIRE(Settle([&saving] { return saving.Finished(); }));
        CHECK(saving.Problems().isEmpty());
    }
    QApplication::processEvents();
    const QImage saved(path);
    REQUIRE_FALSE(saved.isNull());
    CHECK(saved.size() == QSize(1920, 1080));
    CHECK_FALSE(saved.hasAlphaChannel());
    bool drawn = false;
    for (int y = 0; y < saved.height() && !drawn; y += 7) {
        for (int x = 0; x < saved.width() && !drawn; x += 7)
            drawn = saved.pixelColor(x, y) != QColor(0, 0, 0);
    }
    CHECK(drawn);
    CHECK(viewport->grab().toImage() == before);
    CHECK(opening.Problems().isEmpty());
}

TEST_CASE("The work area saves as one PNG per frame, each as the frame saves on its own") {
    const QString game = qEnvironmentVariable("R573_IIDX_DIR");
    if (game.isEmpty()) SKIP("R573_IIDX_DIR not set");
    QSettings().setValue("game/directory", game);
    Editor::Window window;
    QSettings().remove("game/directory");
    window.resize(1600, 900);
    window.show();
    Script opening({});
    window.OpenDocument(game + "/data/graphic/1/title.ifs");
    REQUIRE(opening.Problems().isEmpty());
    auto* timeline = window.findChild<Editor::Timeline*>();
    auto* viewport = window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    QAction* start = ShortcutAction(window, QKeySequence(Qt::Key_B));
    QAction* end = ShortcutAction(window, QKeySequence(Qt::Key_N));
    QAction* save_one = ShortcutAction(window, QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S));
    QAction* save_all =
        ShortcutAction(window, QKeySequence(Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_S));
    REQUIRE(start != nullptr);
    REQUIRE(end != nullptr);
    REQUIRE(save_one != nullptr);
    REQUIRE(save_all != nullptr);
    emit timeline->FrameChosen(kPreviewFrame);
    start->trigger();
    emit timeline->FrameChosen(kPreviewFrame + 2);
    end->trigger();
    emit timeline->FrameChosen(kPreviewFrame + 1);
    QApplication::processEvents();

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    for (uint32_t frame = kPreviewFrame; frame <= kPreviewFrame + 2; frame++) {
        emit timeline->FrameChosen(frame);
        QApplication::processEvents();
        Script saving({PickFile(dir.filePath(QString("single_%1.png").arg(frame)))});
        save_one->trigger();
        REQUIRE(Settle([&saving] { return saving.Finished(); }));
        CHECK(saving.Problems().isEmpty());
    }
    emit timeline->FrameChosen(kPreviewFrame + 1);
    QApplication::processEvents();
    const QImage before = viewport->grab().toImage();
    const QString folder = dir.filePath("frames");
    REQUIRE(QDir().mkpath(folder));
    {
        Script saving({PickFile(folder)});
        save_all->trigger();
        REQUIRE(Settle([&saving] { return saving.Finished(); }));
        INFO(saving.Problems().join("|").toStdString());
        CHECK(saving.Problems().isEmpty());
    }
    QApplication::processEvents();
    const QStringList files = QDir(folder).entryList({"*.png"}, QDir::Files, QDir::Name);
    CHECK(files == QStringList{"title_0400.png", "title_0401.png", "title_0402.png"});
    for (uint32_t frame = kPreviewFrame; frame <= kPreviewFrame + 2; frame++) {
        const QImage saved(
            QDir(folder).filePath(QString("title_%1.png").arg(frame, 4, 10, QChar('0'))));
        REQUIRE_FALSE(saved.isNull());
        CHECK(saved.size() == QSize(1920, 1080));
        CHECK_FALSE(saved.hasAlphaChannel());
        CHECK(saved == QImage(dir.filePath(QString("single_%1.png").arg(frame))));
    }
    CHECK(viewport->grab().toImage() == before);
    window.resize(1500, 880);
    REQUIRE(Settle([&window] { return window.statusBar()->currentMessage().startsWith("Frame"); }));
    CHECK(window.statusBar()->currentMessage().startsWith(
        QString("Frame %1 ").arg(kPreviewFrame + 1)));
    CHECK(opening.Problems().isEmpty());
}

TEST_CASE(
    "The chosen depth's motion path is drawn, in a sprite too, until switched off or hidden") {
    const QString game = qEnvironmentVariable("R573_IIDX_DIR");
    if (game.isEmpty()) SKIP("R573_IIDX_DIR not set");
    const QString title = game + "/data/graphic/1/title.ifs";
    const std::optional<uint16_t> chosen = WidestTitleDepth(title);
    REQUIRE(chosen.has_value());
    if (!chosen) return;
    QSettings().setValue("game/directory", game);
    QSettings().remove("stage/path");
    Editor::Window window;
    QSettings().remove("game/directory");
    window.resize(1600, 900);
    window.show();
    Script opening({});
    window.OpenDocument(title);
    REQUIRE(opening.Problems().isEmpty());
    auto* timeline = window.findChild<Editor::Timeline*>();
    auto* viewport = window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    QAction* path = nullptr;
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->text() == "Motion &path") path = action;
    }
    REQUIRE(path != nullptr);
    CHECK(path->isChecked());
    const auto grab = [&] {
        QApplication::processEvents();
        return viewport->grab().toImage();
    };
    emit timeline->FrameChosen(kPreviewFrame);
    emit timeline->DepthChosen(*chosen);
    const QImage drawn = grab();
    path->trigger();
    const QImage plain = grab();
    CHECK(plain != drawn);
    CHECK_FALSE(QSettings().value("stage/path", true).toBool());
    path->trigger();
    CHECK(grab() == drawn);

    {
        Script hidden({Choose("Hide depth " + QString::number(*chosen) + " in the view")});
        emit timeline->MenuRequested(QPoint(4, 4), kPreviewFrame, QString());
        REQUIRE(Settle([&hidden] { return hidden.Finished(); }));
        CHECK(hidden.Problems().isEmpty());
    }
    const QImage hidden_on = grab();
    path->trigger();
    CHECK(grab() == hidden_on);
    path->trigger();

    const std::optional<uint16_t> sprite_depth = FirstSpriteDepth(title);
    REQUIRE(sprite_depth.has_value());
    auto* clips = window.findChild<QComboBox*>();
    REQUIRE(clips != nullptr);
    clips->setCurrentIndex(1);
    emit timeline->DepthChosen(*sprite_depth);
    REQUIRE(window.statusBar()->currentMessage().endsWith("on its own"));
    const QImage sprite_on = grab();
    path->trigger();
    CHECK(grab() != sprite_on);
    path->trigger();
    CHECK(grab() == sprite_on);
    QSettings().remove("stage/path");
    CHECK(opening.Problems().isEmpty());
}

TEST_CASE("Trimming the title to a work area reloads the host on the kept frames") {
    const QString game = qEnvironmentVariable("R573_IIDX_DIR");
    if (game.isEmpty()) SKIP("R573_IIDX_DIR not set");
    QSettings().setValue("game/directory", game);
    Editor::Window window;
    QSettings().remove("game/directory");
    window.resize(1600, 900);
    window.show();
    Script opening({});
    window.OpenDocument(game + "/data/graphic/1/title.ifs");
    REQUIRE(opening.Problems().isEmpty());
    auto* timeline = window.findChild<Editor::Timeline*>();
    REQUIRE(timeline != nullptr);
    QAction* trim = ShortcutAction(window, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X));
    QAction* start = ShortcutAction(window, QKeySequence(Qt::Key_B));
    QAction* end = ShortcutAction(window, QKeySequence(Qt::Key_N));
    REQUIRE(trim != nullptr);
    REQUIRE(start != nullptr);
    REQUIRE(end != nullptr);
    emit timeline->FrameChosen(kPreviewFrame - 5);
    start->trigger();
    emit timeline->FrameChosen(kPreviewFrame);
    end->trigger();
    emit timeline->FrameChosen(kPreviewFrame - 3);
    {
        Script trimmed({});
        trim->trigger();
        QApplication::processEvents();
        CHECK(trimmed.Problems().isEmpty());
    }
    window.statusBar()->clearMessage();
    window.resize(1500, 880);
    REQUIRE(Settle([&window] { return window.statusBar()->currentMessage().startsWith("Frame"); }));
    INFO(window.statusBar()->currentMessage().toStdString());
    CHECK(window.statusBar()->currentMessage() == "Frame 2 of 6");
    CHECK(opening.Problems().isEmpty());
}

TEST_CASE("Onion skin shows the neighbouring frames and leaves the host on the playhead") {
    const QString game = qEnvironmentVariable("R573_IIDX_DIR");
    if (game.isEmpty()) SKIP("R573_IIDX_DIR not set");
    QSettings().setValue("game/directory", game);
    QSettings().remove("stage/onion");
    Editor::Window window;
    QSettings().remove("game/directory");
    window.resize(1600, 900);
    window.show();
    Script opening({});
    window.OpenDocument(game + "/data/graphic/1/title.ifs");
    REQUIRE(opening.Problems().isEmpty());
    auto* timeline = window.findChild<Editor::Timeline*>();
    auto* viewport = window.findChild<Editor::Viewport*>();
    REQUIRE(timeline != nullptr);
    REQUIRE(viewport != nullptr);
    QAction* onion = nullptr;
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->text() == "&Onion skin") onion = action;
    }
    REQUIRE(onion != nullptr);
    CHECK_FALSE(onion->isChecked());
    emit timeline->FrameChosen(kPreviewFrame + 1);
    QApplication::processEvents();
    const QImage plain = viewport->grab().toImage();

    onion->trigger();
    QApplication::processEvents();
    CHECK(viewport->grab().toImage() != plain);
    window.statusBar()->clearMessage();
    window.resize(1500, 880);
    REQUIRE(Settle([&window] { return window.statusBar()->currentMessage().startsWith("Frame"); }));
    CHECK(window.statusBar()->currentMessage().startsWith(
        QString("Frame %1 ").arg(kPreviewFrame + 1)));

    onion->trigger();
    window.resize(1600, 900);
    QApplication::processEvents();
    REQUIRE(Settle([&] { return viewport->grab().toImage() == plain; }));
    QSettings().remove("stage/onion");
    CHECK(opening.Problems().isEmpty());
}
