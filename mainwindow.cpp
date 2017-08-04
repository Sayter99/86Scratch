#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    serial(new QSerialPort(this))
{
#if defined(Q_OS_MACOS)
    QDir::setCurrent(QCoreApplication::applicationDirPath() + "/..");
#else
    QDir::setCurrent(QCoreApplication::applicationDirPath());
#endif
    ui->setupUi(this);
    setControlsEnable(false);
    detect_serial_port();
    serialDetector = new QTimer(this);
    connect(serialDetector, SIGNAL(timeout()), this, SLOT(detect_serial_port()));
    serialDetector->start(1000);
    updateProjectList();
    ui->importButton->setEnabled(true);
    isRunning = false;
}

MainWindow::~MainWindow()
{
    delete ui;
}

inline void removeInvalidPorts(QList<QSerialPortInfo>& infos)
{
#if defined(Q_OS_MACOS)
    for (int i = 0; i < infos.count(); i++)
    {
        if (infos.at(i).portName().contains("tty"))
            infos.removeAt(i);
    }
#endif
}

void MainWindow::setControlsEnable(bool enable)
{
    ui->deleteButton->setEnabled(enable);
    ui->revertButton->setEnabled(enable);
    ui->importButton->setEnabled(enable);
    ui->uploadFirmwareButton->setEnabled(enable);
    ui->startButton->setEnabled(enable);
    ui->stopButton->setEnabled(enable);
    ui->renameButton->setEnabled(enable);
}

void MainWindow::updateProjectList()
{
    QDir firmwares("./Firmwares");
    QDir helpers("./Helpers");
    QDir origin("./OriginalScratchFiles");
    QDir sb2("./ScratchFiles");
    firmwares.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    helpers.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    origin.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    sb2.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList firmwaresList = firmwares.entryList();
    QStringList helpersList = helpers.entryList();
    QStringList originList = origin.entryList();
    QStringList sb2List = sb2.entryList();

    // check 86Scratch first
    if (firmwaresList.contains("86Scratch") && helpersList.contains("86Scratch") &&
        originList.contains("86Scratch") && sb2List.contains("86Scratch"))
    {
        ui->projectListWidget->addItem("86Scratch");
        ui->projectListWidget->setCurrentRow(0);
    }

    // check other projects
    for (const QString project : firmwaresList)
    {
        if (project != "86Scratch")
        {
            if (helpersList.contains(project) && originList.contains(project) && sb2List.contains(project))
                ui->projectListWidget->addItem(project);
        }
    }
}

bool MainWindow::copyDirRecursively(QString sourceFolder, QString destFolder)
{
    bool success = false;
    QDir sourceDir(sourceFolder);

    if(!sourceDir.exists())
        return false;

    QDir destDir(destFolder);
    if(!destDir.exists())
        destDir.mkdir(destFolder);

    QStringList files = sourceDir.entryList(QDir::Files);
    for(QString file : files)
    {
        QString srcName = sourceFolder + QDir::separator() + file;
        QString destName = destFolder + QDir::separator() + file;
        success = QFile::copy(srcName, destName);
        if(!success)
            return false;
    }

    files.clear();
    files = sourceDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for(QString dir : files)
    {
        QString srcName = sourceFolder + QDir::separator() + dir;
        QString destName = destFolder + QDir::separator() + dir;
        success = copyDirRecursively(srcName, destName);
        if(success == false)
            return false;
    }

    return true;
}

bool MainWindow::containedProject(QString name)
{
    int projectCount = ui->projectListWidget->count();
    for (int i = 0; i < projectCount; i++)
    {
        if (ui->projectListWidget->item(i)->data(0).toString() == name)
            return true;
    }

    return false;
}

void MainWindow::on_projectListWidget_currentTextChanged(const QString &currentText)
{
    if (currentProject == currentText)
        return;
    currentProject = currentText;
    if (currentProject == "86Scratch")
    {
        setControlsEnable(true);
        ui->deleteButton->setEnabled(false);
        ui->renameButton->setEnabled(false);
    }
    else
    {
        setControlsEnable(true);
    }
    ui->firmwaresComboBox->clear();
    QDir firmwares("./Firmwares/" + currentProject);
    QStringList filter("*.exe");
    QStringList firmwareList = firmwares.entryList(filter);
    for (QString &firmware : firmwareList)
    {
        firmware.replace(".exe", "");
        ui->firmwaresComboBox->addItem(firmware);
    }
}

void MainWindow::detect_serial_port()
{
    QString currentPort = ui->serialComboBox->currentText();
    auto infos = QSerialPortInfo::availablePorts();
    removeInvalidPorts(infos);
    for (int i = 0; i < ui->serialComboBox->count(); i++)
    {
        bool isContain = false;
        for (const QSerialPortInfo &info : infos)
        {
            if (info.portName() == ui->serialComboBox->itemText(i))
                isContain = true;
        }
        if (isContain == false)
            ui->serialComboBox->removeItem(i);
    }
    for (const QSerialPortInfo &info : infos)
    {
        if (ui->serialComboBox->findText(info.portName()) == -1)
            ui->serialComboBox->addItem(info.portName());
    }
    auto findIndex = ui->serialComboBox->findText(currentPort);
    if (ui->serialComboBox->count() == 1)
        ui->serialComboBox->setCurrentIndex(0);
    else
        ui->serialComboBox->setCurrentIndex(findIndex);
}

void MainWindow::warnMessage(QString text)
{
    QMessageBox warningBox;
    warningBox.setText(text);
    warningBox.exec();
}

QString MainWindow::scanUploadPort()
{
    QStringList currentPorts;
    auto infos = QSerialPortInfo::availablePorts();
    removeInvalidPorts(infos);
    for (const QSerialPortInfo &info : infos)
        currentPorts.push_back(info.portName());

    QTime t;
    t.start();
    while(t.elapsed() < 8000)
    {
        infos = QSerialPortInfo::availablePorts();
        removeInvalidPorts(infos);
        for (const QSerialPortInfo &info : infos)
        {
            if (!currentPorts.contains(info.portName()))
                return info.portName();
        }
        QThread::msleep(100);
    }
    return "";
}

void MainWindow::waitForConnected(QString prevPortName)
{
    QTime t;
    t.start();
    while(t.elapsed() < 8000)
    {
        bool completed = false;
        auto infos = QSerialPortInfo::availablePorts();
        removeInvalidPorts(infos);
        ui->serialComboBox->clear();
        for (const QSerialPortInfo &info : infos)
        {
            ui->serialComboBox->addItem(info.portName());
            if (info.portName() == prevPortName)
            {
                ui->serialComboBox->setCurrentText(prevPortName);
                completed = true;
            }
        }
        if (completed)
            return;
        QThread::msleep(100);
    }
}

void MainWindow::on_uploadFirmwareButton_clicked()
{
    QString firmware = ui->firmwaresComboBox->currentText();
    if (firmware == "")
    {
        warnMessage("Error: Missing a valid firmware");
        return;
    }
    else
    {
        firmware = "./Firmwares/" + currentProject + "/" + firmware + ".exe";
    }
    QString portName = ui->serialComboBox->currentText();
    if (portName == "")
    {
        warnMessage("Error: Missing a valid serial port");
        return;
    }

    serialDetector->stop();
    ui->statusLabel->setStyleSheet("QLabel {color : red;}");
    ui->statusLabel->setText("Uploading");
    qApp->processEvents();

    QProcess reseter;
    QProcess uploader;
    QString uploadPort;
#if defined(Q_OS_LINUX) && defined(Q_PROCESSOR_X86_32)
    QFile reset86("./uploader/reset_linux32");
    QFile v86dude("./uploader/v86dude_linux32");
    if (v86dude.exists() && reset86.exists())
    {
        reseter.start(reset86.fileName(), QStringList() << "/dev/" + portName);
        reseter.waitForFinished();
        uploadPort = scanUploadPort();
        if (uploadPort != "")
        {
            uploader.setWorkingDirectory(QDir::currentPath());
            uploader.start(v86dude.fileName(), QStringList() << "/dev/" + uploadPort << "20" << firmware);
            uploader.waitForFinished();
        }
    }
    else
    {
        warnMessage("Error: Missing the uploader");
        ui->statusLabel->setStyleSheet("QLabel {color : black;}");
        ui->statusLabel->setText("Disconnected");
    }
#elif defined(Q_OS_LINUX) && defined(Q_PROCESSOR_X86_64)
    QFile reset86("./uploader/reset_linux64");
    QFile v86dude("./uploader/v86dude_linux64");
    if (v86dude.exists() && reset86.exists())
    {
        reseter.start(reset86.fileName(), QStringList() << "/dev/" + portName);
        reseter.waitForFinished();
        uploadPort = scanUploadPort();
        if (uploadPort != "")
        {
            uploader.setWorkingDirectory(QDir::currentPath());
            uploader.start(v86dude.fileName(), QStringList() << "/dev/" + uploadPort << "20" << firmware);
            uploader.waitForFinished();
        }
    }
    else
    {
        warnMessage("Error: Missing the uploader");
        ui->statusLabel->setStyleSheet("QLabel {color : black;}");
        ui->statusLabel->setText("Disconnected");
    }
#elif defined(Q_OS_MACOS)
    QFile reset86("./uploader/reset_macosx.exe");
    QFile v86dude("./uploader/v86dude_mac.exe");
    if (v86dude.exists() && reset86.exists())
    {
        reseter.start(reset86.fileName(), QStringList() << "/dev/" + portName);
        reseter.waitForFinished();
        uploadPort = scanUploadPort();
        if (uploadPort != "")
        {
            uploader.setWorkingDirectory(QDir::currentPath());
            uploader.start(v86dude.fileName(), QStringList() << "/dev/" + uploadPort << "20" << firmware);
            uploader.waitForFinished();
        }
    }
    else
    {
        warnMessage("Error: Missing the uploader");
        ui->statusLabel->setStyleSheet("QLabel {color : black;}");
        ui->statusLabel->setText("Disconnected");
    }
#elif defined(Q_OS_WIN)
    QFile reset86("./uploader/reset_win.exe");
    QFile v86dude("./uploader/v86dude_win.exe");
    if (v86dude.exists() && reset86.exists())
    {
        reseter.start(reset86.fileName(), QStringList() << portName);
        reseter.waitForFinished();
        uploadPort = scanUploadPort();
        if (uploadPort != "")
        {
            uploader.setWorkingDirectory(QDir::currentPath());
            uploader.start(v86dude.fileName(), QStringList() << uploadPort << "20" << firmware);
            uploader.waitForFinished();
        }
    }
    else
    {
        warnMessage("Error: Missing the uploader");
        ui->statusLabel->setStyleSheet("QLabel {color : black;}");
        ui->statusLabel->setText("Disconnected");
    }
#endif
    if (uploadPort != "")
    {
        waitForConnected(portName);
        ui->statusLabel->setStyleSheet("QLabel {color : green;}");
        ui->statusLabel->setText("Done");
    }
    else
    {
        warnMessage("Error: Failed to upload the firmware");
        ui->statusLabel->setStyleSheet("QLabel {color : black;}");
        ui->statusLabel->setText("Disconnected");
    }
    serialDetector->start(1000);
}

void MainWindow::launchScratch(const QDir& sb2, const QStringList& sb2Files)
{
    QString sb2File = sb2.absolutePath() + "/" + sb2Files.at(0);
    scratch.setWorkingDirectory(sb2.absolutePath());
#if defined(Q_OS_LINUX) && defined(Q_PROCESSOR_X86_32)
    warnMessage("You can start Scratch 2.0 now!");
#elif defined(Q_OS_LINUX) && defined(Q_PROCESSOR_X86_64)
    warnMessage("You can start Scratch 2.0 now!");
#elif defined(Q_OS_MACOS)
    QString scratchPath = "/Applications/Scratch 2.app/Contents/MacOS/Scratch 2";
    QFile scratchApp(scratchPath);
    if (!scratchApp.exists())
    {
        warnMessage("Error: Scratch 2 not found in Applications");
        return;
    }
    scratch.start(scratchPath, QStringList() << sb2File);
#elif defined(Q_OS_WIN)
    QSettings m("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall", QSettings::NativeFormat);
    QStringList softwares = m.childGroups();
    for (const QString &software : softwares)
    {
        m.beginGroup(software);
        if (m.value("DisplayName").toString() == "Scratch 2 Offline Editor")
        {
            QString path = m.value("installLocation").toString();
            scratch.start(path + "/Scratch 2.exe", QStringList() << sb2File);
            return;
        }
        m.endGroup();
    }
    warnMessage("Error: Scratch 2 not found");
#endif
}

void MainWindow::on_startButton_clicked()
{
    if (isRunning)
    {
        warnMessage("Error: Please stop first");
        return;
    }
    QFile helper("./Helpers/" + currentProject + "/s2a_fm.py");
    if (!helper.exists())
    {
        warnMessage("Error: Missing the Python Helper");
        return;
    }
    else
    {
        QString portName = ui->serialComboBox->currentText();
        if (portName == "")
        {
            warnMessage("Error: Missing a valid serial port");
            return;
        }

#if defined(Q_OS_WIN)
        portName = portName;
#else
        portName = "/dev/" + portName;
#endif
        ui->statusLabel->setStyleSheet("QLabel {color : blue;}");
        ui->statusLabel->setText("Connecting");
        // launch python helper
        QString helperDir = QDir::currentPath() + "/Helpers/" + currentProject;
        s2a_fm.setWorkingDirectory(helperDir);
        s2a_fm.start("python", QStringList() << "s2a_fm.py" << portName);
        if (s2a_fm.error() == QProcess::FailedToStart)
        {
            warnMessage("Error: Cannot launch helper. Please install Python 2.7 or check the environment variables");
            ui->statusLabel->setStyleSheet("QLabel {color : black;}");
            ui->statusLabel->setText("Disconnected");
            return;
        }

        QDir sb2("./ScratchFiles/" + currentProject);
        QStringList filter("*.sb2");
        QStringList sb2Files = sb2.entryList(filter);
        if (sb2Files.count() == 0)
        {
            warnMessage("Error: Missing sb2 files");
            ui->statusLabel->setStyleSheet("QLabel {color : black;}");
            ui->statusLabel->setText("Disconnected");
            return;
        }
        launchScratch(sb2, sb2Files);
        ui->statusLabel->setStyleSheet("QLabel {color : green;}");
        ui->statusLabel->setText("Connected");
        ui->deleteButton->setEnabled(false);
        ui->revertButton->setEnabled(false);
        ui->uploadFirmwareButton->setEnabled(false);
        isRunning = true;
    }
}

void MainWindow::on_stopButton_clicked()
{
    if (isRunning == false)
    {
        warnMessage("Error: Please start first");
        return;
    }
    if (currentProject != "86Scratch")
        ui->deleteButton->setEnabled(true);
    ui->uploadFirmwareButton->setEnabled(true);
    ui->revertButton->setEnabled(true);
    ui->statusLabel->setStyleSheet("QLabel {color : black;}");
    ui->statusLabel->setText("Disconnected");
    s2a_fm.close();
    scratch.close();
    isRunning = false;
}

void MainWindow::on_revertButton_clicked()
{
    QDir scratchFiles("./ScratchFiles/");
    QDir current("./ScratchFiles/" + currentProject);
    QDir origin("./OriginalScratchFiles/" + currentProject);
    if (current.exists() && origin.exists())
    {
        current.removeRecursively();
        scratchFiles.mkdir(currentProject);
        QStringList filter("*.sb2");
        QStringList originFiles = origin.entryList(filter);
        for (const QString filename : originFiles)
        {
            QFile orgsb2(origin.absolutePath() + "/" + filename);
            orgsb2.copy(current.absolutePath() + "/" + filename);
        }
        warnMessage("Revert " + currentProject + " successfully");
    }
    else
    {
        warnMessage("Error: Missing sb2 directories");
    }
}

void MainWindow::on_deleteButton_clicked()
{
    QDir firmwares("./Firmwares/" + currentProject);
    QDir helpers("./Helpers/" + currentProject);
    QDir origin("./OriginalScratchFiles/" + currentProject);
    QDir sb2("./ScratchFiles/" + currentProject);
    firmwares.removeRecursively();
    helpers.removeRecursively();
    origin.removeRecursively();
    sb2.removeRecursively();
    ui->projectListWidget->takeItem(ui->projectListWidget->currentRow());
}

QString MainWindow::defaultProjectName(QString defaultBase)
{
    int serialNum = 1;
    int projectCount = ui->projectListWidget->count();
    for (int i = 0; i < projectCount; i++)
    {
        if (ui->projectListWidget->item(i)->data(0).toString() == defaultBase + QString::number(serialNum))
        {
            serialNum++;
            i = 0;
            continue;
        }
    }
    return defaultBase + QString::number(serialNum);
}

void MainWindow::on_importButton_clicked()
{
    QSettings setting;
    QString importPath = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
                                                           setting.value("default_dir").toString(),
                                                           QFileDialog::ShowDirsOnly |
                                                           QFileDialog::DontResolveSymlinks);
    if (!importPath.isEmpty())
    {
        QDir importDir(importPath);
        QRegularExpression re("[0-9A-Za-z]+");
        QRegularExpressionValidator v(re);
        int pos = 0;
        QString projectName = importDir.dirName();
        if (v.validate(projectName, pos) != QRegularExpressionValidator::Acceptable)
            projectName = defaultProjectName();
        else if (containedProject(projectName))
            projectName = defaultProjectName(projectName);

        QDir projectDir;
        projectDir.mkdir("./Firmwares/" + projectName);
        projectDir.mkdir("./Helpers/" + projectName);
        projectDir.mkdir("./OriginalScratchFiles/" + projectName);
        projectDir.mkdir("./ScratchFiles/" + projectName);
        QDir firmwares("./Firmwares/" + projectName);
        QDir helpers("./Helpers/" + projectName);
        QDir origin("./OriginalScratchFiles/" + projectName);
        QDir sb2("./ScratchFiles/" + projectName);

        // 86Duino Firmware
        QStringList filter("*.exe");
        QStringList filenames = importDir.entryList(filter);
        for (QString filename : filenames)
        {
            QFile copiedFile(importDir.absolutePath() + "/" + filename);
            copiedFile.copy(firmwares.absolutePath() + "/" + filename);
        }
        // Robot86ME.sb2
        filter.clear();
        filter.append("Robot86ME.sb2");
        filenames = importDir.entryList(filter);
        for (QString filename : filenames)
        {
            QFile copiedFile(importDir.absolutePath() + "/" + filename);
            copiedFile.copy(sb2.absolutePath() + "/" + filename);
            copiedFile.copy(origin.absolutePath() + "/" + filename);
        }
        // scratch_command_handlers.py
        copyDirRecursively(QDir::currentPath() + "/Helpers/86Scratch", helpers.absolutePath());
        QFile oldScratchCommandHandlers(helpers.absolutePath() + "/scratch_command_handlers.py");
        oldScratchCommandHandlers.remove();
        filter.clear();
        filter.append("scratch_command_handlers.py");
        filenames = importDir.entryList(filter);
        for (QString filename : filenames)
        {
            QFile copiedFile(importDir.absolutePath() + "/" + filename);
            copiedFile.copy(helpers.absolutePath() + "/" + filename);
        }

        ui->projectListWidget->addItem(projectName);
        ui->projectListWidget->setCurrentRow(ui->projectListWidget->count() - 1);
    }
}

void MainWindow::on_renameButton_clicked()
{
    bool ok;
    QString newName = QInputDialog::getText(0, "Rename",
                                            "Enter the new name:", QLineEdit::Normal,
                                            currentProject, &ok);
    if (ok && !newName.isEmpty())
    {
        QRegularExpression re("[0-9A-Za-z]+");
        QRegularExpressionValidator v(re);
        int pos = 0;
        if (v.validate(newName, pos) != QRegularExpressionValidator::Acceptable)
        {
            warnMessage("Error: Please enter the name without spaces and special characters");
            return;
        }
        if (newName != currentProject)
        {
            if (containedProject(newName))
            {
                warnMessage("Error: This name is already existed");
                return;
            }
            ui->projectListWidget->selectedItems().at(0)->setData(0, QVariant(newName));
            QDir firmwares("./Firmwares/" + currentProject);
            QDir helpers("./Helpers/" + currentProject);
            QDir origin("./OriginalScratchFiles/" + currentProject);
            QDir sb2("./ScratchFiles/" + currentProject);
            firmwares.rename(firmwares.absolutePath(), QDir::currentPath() + "/Firmwares/" + newName);
            helpers.rename(helpers.absolutePath(), QDir::currentPath() + "/Helpers/" + newName);
            origin.rename(origin.absolutePath(), QDir::currentPath() + "/OriginalScratchFiles/" + newName);
            sb2.rename(sb2.absolutePath(), QDir::currentPath() + "/ScratchFiles/" + newName);
            currentProject = newName;
        }
    }
}

// override closeEvent of MainWindow
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (isRunning)
    {
        s2a_fm.close();
        scratch.close();
        isRunning = false;
    }
    event->accept();
}
