#include "centralwidget.h"

CentralWidget::CentralWidget(std::shared_ptr<DocumentWidget> _docWidget, std::shared_ptr<FolderViewProxy> _folderView, QWidget *parent)
    : QStackedWidget(parent),
      documentView(_docWidget),
      folderView(_folderView)
{
    setMouseTracking(true);
    if(!documentView || !folderView)
        qDebug() << "[CentralWidget] Error: child widget is null. We will crash now.  Bye.";

    // docWidget - 0, folderView - 1
    addWidget(documentView.get());
    if(folderView)
        addWidget(folderView.get());
    showDocumentView();
}

void CentralWidget::showDocumentView() {
    if(mode == MODE_DOCUMENT)
        return;
    mode = MODE_DOCUMENT;
    setCurrentIndex(0);
    widget(0)->setFocus();
    documentView->viewWidget()->startPlayback();
    emit viewModeChanged(mode);
}

void CentralWidget::showFolderView() {
    if(mode == MODE_FOLDERVIEW)
        return;

    mode = MODE_FOLDERVIEW;
    setCurrentIndex(1);
    widget(1)->show();
    widget(1)->setFocus();
    documentView->viewWidget()->stopPlayback();
    emit viewModeChanged(mode);
}

CollageWidget *CentralWidget::ensureCollage() {
    if(!collage) {
        collage = new CollageWidget(this);
        addWidget(collage);
    }
    return collage;
}

CollageWidget *CentralWidget::collageWidget() const {
    return collage;
}

void CentralWidget::showCollageView() {
    if(mode == MODE_COLLAGE)
        return;
    ensureCollage();
    mode = MODE_COLLAGE;
    setCurrentWidget(collage);
    collage->setFocus();
    documentView->viewWidget()->stopPlayback();
    emit viewModeChanged(mode);
}

void CentralWidget::toggleViewMode() {
    // from the collage this goes back to the document view
    (mode == MODE_DOCUMENT) ? showFolderView() : showDocumentView();
}

ViewMode CentralWidget::currentViewMode() {
    return mode;
}
