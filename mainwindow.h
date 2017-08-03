#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QtSerialPort/QSerialPortInfo>
#include <QMessageBox>
#include <QSerialPort>
#include <QProcess>
#include <QtGlobal>
#include <QThread>
#include <QSettings>
#include <QCloseEvent>
#include <QRegularExpression>
#include <QValidator>
#include <QInputDialog>
#include <QFileDialog>

namespace Ui
{
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void detect_serial_port();
    void on_uploadFirmwareButton_clicked();
    void on_startButton_clicked();
    void on_stopButton_clicked();
    void on_revertButton_clicked();
    void on_deleteButton_clicked();
    void on_importButton_clicked();
    void on_renameButton_clicked();
    void on_projectListWidget_currentTextChanged(const QString &currentText);

private:
    Ui::MainWindow *ui;
    QTimer *serialDetector;
    QString currentProject;
    QSerialPort *serial;
    QProcess s2a_fm;
    QProcess scratch;
    bool isRunning;
    void warnMessage(QString text);
    void updateProjectList();
    void setControlsEnable(bool enable);
    QString scanUploadPort();
    void waitForConnected(QString prevPortName);
    void launchScratch(const QDir& sb2, const QStringList& sb2Files);
    bool copyDirRecursively(QString sourceFolder, QString destFolder);
    QString defaultProjectName(QString defaultBase = "NewProject");
    bool containedProject(QString name);
    void closeEvent(QCloseEvent *event);
};

#endif // MAINWINDOW_H
