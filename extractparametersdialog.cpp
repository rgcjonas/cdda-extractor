// Copyright © 2020 Jonas Kümmerlin <jonas@kuemmerlin.eu>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include "extractparametersdialog.h"
#include "ui_extractparametersdialog.h"
#include "uiutil/win32iconloader.h"
#include "fileutil.h"

#include <QStandardPaths>
#include <QFileDialog>
#include <QDir>
#include <QIcon>
#include <QSettings>

ExtractParametersDialog::ExtractParametersDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ExtractParametersDialog)
{
    ui->setupUi(this);

    ui->eOutputDirectory->setText(QStandardPaths::writableLocation(QStandardPaths::MusicLocation));

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

#ifdef Q_OS_WIN32
    QIcon themeFolder = IconLoader::fromShellStock(SIID_FOLDER);
#else
    QIcon themeFolder = QIcon::fromTheme(QStringLiteral("folder"));
#endif
    if (!themeFolder.isNull())
        ui->iwFolder->setIcon(themeFolder);
    else
        ui->iwFolder->setIcon(QIcon(QStringLiteral(":/inode-directory.svg")));

#ifdef Q_OS_WIN32
    QIcon themeFormat = IconLoader::fromShellStock(SIID_AUDIOFILES);
#else
    QIcon themeFormat = QIcon::fromTheme(QStringLiteral("audio-x-generic"));
#endif
    if (!themeFormat.isNull())
        ui->iwFileFormat->setIcon(themeFormat);
    else
        ui->iwFileFormat->setIcon(QIcon(QStringLiteral(":/audio-x-generic.svg")));

    ui->iwFolder->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    ui->iwFileFormat->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    QSettings settings;
    QString dir = settings.value(QStringLiteral("Output Directory")).toString();
    if (dir.size() && QDir(dir).exists()) {
        ui->eOutputDirectory->setText(dir);
    }
    ui->cbCreateSubfolder->setChecked(settings.value(QStringLiteral("Create Subfolder"), bool(true)).toBool());

    QString format = settings.value(QStringLiteral("Format"), QStringLiteral("flac")).toString();
    if (format == QLatin1String("flac")) {
        ui->rbFlac->setChecked(true);
    } else if (format == QLatin1String("mp3") && ui->rbMp3->isEnabled()) {
        ui->rbMp3->setChecked(true);
    } else if (format == QLatin1String("wav")) {
        ui->rbWav->setChecked(true);
    }
}

ExtractParametersDialog::~ExtractParametersDialog()
{
    delete ui;
}

void ExtractParametersDialog::setAlbumInfo(const QString &title, const QString &artist)
{
    m_album = title;
    m_artist = artist;
    ui->cbCreateSubfolder->setText(tr("Create Subfolder: %1/%2").arg(FileUtil::sanitizeFilename(artist)).arg(FileUtil::sanitizeFilename(title)));
}

QString ExtractParametersDialog::outputDir() const
{
    QString dir = ui->eOutputDirectory->text();
    if (ui->cbCreateSubfolder->isChecked())
        dir = QStringLiteral("%1/%2/%3").arg(dir, FileUtil::sanitizeFilename(m_artist), FileUtil::sanitizeFilename(m_album));

    return dir;
}

QString ExtractParametersDialog::format() const
{
    if (ui->rbFlac->isChecked())
        return QStringLiteral("flac");
    if (ui->rbMp3->isChecked())
        return QStringLiteral("mp3");

    return QStringLiteral("wav");
}

bool ExtractParametersDialog::paranoiaActivated() const
{
    QSettings settings;
    return settings.value(QStringLiteral("Paranoia Mode"), QStringLiteral("paranoia")).toString() == QStringLiteral("paranoia");
}

void ExtractParametersDialog::on_bBrowseDir_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    tr("Select output directory"),
                                                    ui->eOutputDirectory->text());
    if (dir.size())
        ui->eOutputDirectory->setText(dir);
}

void ExtractParametersDialog::on_bStart_clicked()
{
    QSettings settings;
    settings.setValue(QStringLiteral("Output Directory"), ui->eOutputDirectory->text());
    settings.setValue(QStringLiteral("Create Subfolder"), ui->cbCreateSubfolder->isChecked());
    settings.setValue(QStringLiteral("Format"), format());

    accept();
}


void ExtractParametersDialog::showEvent(QShowEvent *event)
{
    resize(width(), heightForWidth(width()));

    QDialog::showEvent(event);
}
