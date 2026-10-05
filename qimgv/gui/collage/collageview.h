#pragma once

#include <QGraphicsView>
#include <QStringList>
#include "gui/collage/collagescene.h"

// Canvas viewport: wheel zooms the view, middle-drag pans, files can be dropped in.
// View mode: the scene is sized to this viewport, so a re-flow follows every resize.
class CollageView : public QGraphicsView {
    Q_OBJECT
public:
    explicit CollageView(CollageScene *scene, QWidget *parent = nullptr);

    bool isAutoFit() const;
    void setViewMode(bool mode);
    bool isViewMode() const;

public slots:
    void fitCanvas();
    void zoomToActualSize();

signals:
    void filesDropped(const QStringList &paths);
    void modeToggleRequested(); // "E" key
    void animationToggleRequested(); // Space with no animated tile selected

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    CollageScene *mScene;
    bool mAutoFit = true;
    bool mPanning = false;
    bool mViewMode = false;

    void viewScaleChanged();
    void applyDragMode();
    void updateViewArea();
    CollageItem *itemUnderCursor(const QPoint &pos) const;
    void zoomBy(qreal factor);
    void nudgeSelection(int dx, int dy);
};
