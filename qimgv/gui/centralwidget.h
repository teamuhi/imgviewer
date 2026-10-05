#pragma once

#include <QStackedWidget>
#include "gui/folderview/folderviewproxy.h"
#include "gui/viewers/documentwidget.h"
#include "gui/collage/collagewidget.h"
#include "settings.h"


class CentralWidget : public QStackedWidget
{
    Q_OBJECT
public:
    explicit CentralWidget(std::shared_ptr<DocumentWidget> _docWidget, std::shared_ptr<FolderViewProxy> _folderView, QWidget *parent = nullptr);

    ViewMode currentViewMode();
    // created on first use, so the collage costs nothing until it is opened
    CollageWidget *ensureCollage();
    CollageWidget *collageWidget() const;
signals:
    void viewModeChanged(ViewMode mode);

public slots:
    void showDocumentView();
    void showFolderView();
    void toggleViewMode();
    void showCollageView();

private:
    std::shared_ptr<DocumentWidget> documentView;
    std::shared_ptr<FolderViewProxy> folderView;
    CollageWidget *collage = nullptr;
    ViewMode mode = MODE_FOLDERVIEW; // so the first showDocumentView() always runs
};
