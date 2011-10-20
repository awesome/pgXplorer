#include "queryview.h"

ulong QueryView::queryViewObjectId = 0;

QueryView::QueryView(QWidget *parent, QSqlQueryModel* model, QString const name,
                     int const time, int const rows, int const cols, Qt::WidgetAttribute f)
{
    //Identify this object with thisTableViewId for constructing database connection
    //specific to this object and this object alone.
    thisQueryViewId = queryViewObjectId++;

    //Thread busy indicator to avoid crashing of app when
    //QueryView receives a close signal before data retrieval.
    threadBusy = false;

    qview = new QTableView(this);
    qview->resizeColumnsToContents();
    setCentralWidget(qview);
    QString timeel = QApplication::translate("QueryView", "Time elapsed:", 0, QApplication::UnicodeUTF8);
    QString rowsStr = QApplication::translate("QueryView", "Rows:", 0, QApplication::UnicodeUTF8);
    QString colsStr = QApplication::translate("QueryView", "Columns:", 0, QApplication::UnicodeUTF8);
    statusBar()->showMessage(QApplication::translate("QueryView", "Fetching data...", 0, QApplication::UnicodeUTF8));
    //qview->setModel(model);
    setWindowTitle(name);
    qview->setStyleSheet("QTableView {font-weight: 400;}");
    qview->setAlternatingRowColors(true);
    setGeometry(100,100,640,480);

    //Create Ctrl+Shift+C key combo to copy selected table contents without headers.
    QShortcut* shortcut_ctrl_c = new QShortcut(QKeySequence::Copy, this);
    connect(shortcut_ctrl_c, SIGNAL(activated()), this, SLOT(copyc()));

    //Create Ctrl+Shift+C key combo to copy selected table contents with headers.
    QShortcut* shortcut_ctrl_shft_c = new QShortcut(QKeySequence("Ctrl+Shift+C"), this);
    connect(shortcut_ctrl_shft_c, SIGNAL(activated()), this, SLOT(copych()));

    //Create key-sequences for fullscreen and restore.
    QShortcut* shortcut_fs_win = new QShortcut(QKeySequence::QKeySequence(Qt::Key_F11), this);
    connect(shortcut_fs_win, SIGNAL(activated()), this, SLOT(fullscreen()));
    QShortcut* shortcut_restore_win = new QShortcut(QKeySequence::QKeySequence(Qt::Key_Escape), this);
    connect(shortcut_restore_win, SIGNAL(activated()), this, SLOT(restore()));
    show();
}

QueryView::~QueryView()
{
    //Clean-up only when there is no active thread.
    //However, this will cause a memory leak when the
    //QueryView is closed when the thread is busy.
    //Proper solution is to create a Thread class
    //and cancel that before we clean-up. We cannot do
    //this now because we are using QFuture (per Qt docs).
    if(!threadBusy)
    {
        QSqlQueryModel* sqlqmt = getMod();
        delete qview;
        delete sqlqmt;
        close();
    }
}

void QueryView::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;

    QPalette palette;
    palette.setColor(menu.backgroundRole(), QColor(205,205,205));
    menu.setPalette(palette);

    menu.addAction("Remove");
    QAction *a = menu.exec(event->screenPos());
    if(a && QString::compare(a->text(),"Remove")==0) {

    }
    if(a && QString::compare(a->text(),"Expand")==0) {

    }
    if(a && QString::compare(a->text(),"Collapse")==0) {

    }
}

void QueryView::closeEvent(QCloseEvent *event)
{
    if(!threadBusy)
    {
        emit cleanSignal();
        delete qview;
        close();
    }
}

void QueryView::copyc()
{
    QItemSelectionModel* s = qview->selectionModel();
    QModelIndexList indices = s->selectedIndexes();
    if(indices.isEmpty()) {
        return;
    }
    qSort(indices);
    QModelIndex prev = indices.first();
    QModelIndex last = indices.last();
    indices.removeFirst();
    QModelIndex current;
    QString selectedText;

    foreach(current, indices) {
        QVariant data = qview->model()->data(prev);
        selectedText.append(data.toString());
        if(current.row() != prev.row())
            selectedText.append(QLatin1Char('\n'));
        else
            selectedText.append(QLatin1Char('\t'));
        prev = current;
    }
    selectedText.append(qview->model()->data(last).toString());
    selectedText.append(QLatin1Char('\n'));
    qApp->clipboard()->setText(selectedText);
}

void QueryView::copych()
{
    QAbstractItemModel* atm = qview->model();
    QItemSelectionModel* s = qview->selectionModel();
    QModelIndexList indices = s->selectedIndexes();
    if(indices.isEmpty())
        return;
    qSort(indices);
    QString headerText;
    QModelIndex current;
    int prevRow = indices.at(0).row();
    foreach(current, indices) {
        if(current.row() == prevRow) {
            QVariant data = atm->headerData(current.column(), Qt::Horizontal);
            headerText.append(data.toString());
            headerText.append(QLatin1Char('\t'));
        }
        else {
            headerText.append(QLatin1Char('\n'));
            break;
        }
        prevRow = current.row();
    }
    if(!headerText.endsWith("\n"))
        headerText.append(QLatin1Char('\n'));
    QString selectedText;
    QModelIndex prev = indices.first();
    QModelIndex last = indices.last();
    indices.removeFirst();
    foreach(current, indices) {
        QVariant data = atm->data(prev);
        selectedText.append(data.toString());
        if(current.row() != prev.row())
            selectedText.append(QLatin1Char('\n'));
        else
            selectedText.append(QLatin1Char('\t'));
        prev = current;
    }
    selectedText.append(atm->data(last).toString());
    selectedText.append(QLatin1Char('\n'));
    qApp->clipboard()->setText(headerText + selectedText);
}

void QueryView::fetchDataSlot(SqlMdl* smdl, int time, qint32 rows, qint32 cols)
{
    if(smdl->lastError().isValid()) {
        //emit errMesg(smdl->lastError().databaseText(), 1);
        //return;

        QStandardItemModel* model = new QStandardItemModel(0,1);
        QStringList mesgs = smdl->lastError().databaseText().split("\n");
        QStandardItem *item = new QStandardItem(mesgs[0]);
        model->appendRow(item);
        item->appendRow(new QStandardItem(mesgs[1]));
        model->appendRow(new QStandardItem(mesgs[1]));
        item->appendRow(new QStandardItem(mesgs[2]));
        model->appendRow(new QStandardItem(mesgs[2]));
        QFont serifFont("Courier", 10, QFont::Bold);
        qview->setFont(serifFont);
        qview->setModel(model);
        qview->resizeColumnToContents(0);
        QStringList hdr;
        hdr << "Error messages";
        model->setHorizontalHeaderLabels(hdr);
        QString timeel = QApplication::translate("QueryView", "Time elapsed:", 0, QApplication::UnicodeUTF8);
        QString rowsStr = QApplication::translate("QueryView", "Rows:", 0, QApplication::UnicodeUTF8);
        QString colsStr = QApplication::translate("QueryView", "Columns:", 0, QApplication::UnicodeUTF8);
        statusBar()->showMessage(timeel + QString::number((double)time/1000) +
                                 " s \t " + rowsStr + "3" +
                                 " \t " + colsStr + "1");
    }
    else
    {
        QString timeel = QApplication::translate("QueryView", "Time elapsed:", 0, QApplication::UnicodeUTF8);
        QString rowsStr = QApplication::translate("QueryView", "Rows:", 0, QApplication::UnicodeUTF8);
        QString colsStr = QApplication::translate("QueryView", "Columns:", 0, QApplication::UnicodeUTF8);
        statusBar()->showMessage(timeel + QString::number((double)time/1000) +
                                 " s \t " + rowsStr + QString::number(rows) +
                                 " \t " + colsStr + QString::number(cols));
        qview->setModel(smdl);
    }
    setCursor(Qt::ArrowCursor);
    threadBusy = false;
    emit finished();
}

void QueryView::busySlot()
{
    threadBusy = true;
    setCursor(Qt::WaitCursor);
}

void QueryView::fullscreen()
{
    this->showFullScreen();
}

void QueryView::restore()
{
    this->showNormal();
}

