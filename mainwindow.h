#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include "notelistwidget.h"
#include "noteeditwidget.h"
#include "notedatabase.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onNoteSelected(const Note &note);
    void onCreateNewNote();
    void onNoteSaved(const Note &note);
    void onNoteDeleted(int noteId);
    void onEditWindowClosed();
    void onNoteEditWindowClosed(int noteId);
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MainWindow *ui;
    NoteListWidget *m_noteListWidget;
    NoteEditWidget *m_noteEditWidget;
    NoteDatabase *m_database;
    QMap<int, NoteEditWidget*> m_openNoteWindows;
    
    void setupUI();
    void setupConnections();
    void setupAppearance();
    
    void openOrActivateNoteWindow(const Note &note);
    void closeAllNoteWindows();
};
#endif // MAINWINDOW_H
