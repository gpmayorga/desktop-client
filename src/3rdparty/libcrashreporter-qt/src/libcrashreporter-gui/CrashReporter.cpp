/* === This file is part of Tomahawk Player - <http://tomahawk-player.org> ===
 *
 *   Copyright 2010-2011, Christian Muehlhaeuser <muesli@tomahawk-player.org>
 *
 *   Tomahawk is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Tomahawk is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Tomahawk. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CrashReporter.h"

#include <QIcon>
#include <QDebug>
#include <QTimer>
#include <QDir>
#include <QDateTime>

// #include "utils/TomahawkUtils.h"

#include "ui_CrashReporter.h"


// #define LOGFILE TomahawkUtils::appLogDir().filePath( "Tomahawk.log" ).toLocal8Bit()
#define RESPATH ":/data/"
#define PRODUCT_NAME "WaterWolf"

CrashReporter::CrashReporter( const QUrl& url, const QStringList& args )
: m_ui( 0 )
, m_reply( 0 )
, m_url( url )
{
    m_ui = new Ui::CrashReporter();
    m_ui->setupUi( this );
    m_ui->progressBar->setRange( 0, 100 );
    m_ui->progressBar->setValue( 0 );
    m_ui->progressLabel->setPalette( Qt::gray );

    #ifdef Q_OS_MAC
    QFont f = m_ui->bottomLabel->font();
    f.setPointSize( 10 );
    m_ui->bottomLabel->setFont( f );
    f.setPointSize( 11 );
    m_ui->progressLabel->setFont( f );
    m_ui->progressLabel->setIndent( 3 );
    #else
    m_ui->vboxLayout->setSpacing( 16 );
    m_ui->hboxLayout1->setSpacing( 16 );
    m_ui->progressBar->setTextVisible( false );
    m_ui->progressLabel->setIndent( 1 );
    m_ui->bottomLabel->setDisabled( true );
    m_ui->bottomLabel->setIndent( 1 );
    #endif //Q_OS_MAC

    m_request = new QNetworkRequest( m_url );

    m_minidump_file_path = args.value( 1 );

    //hide until "send report" has been clicked
    m_ui->progressBar->setVisible( false );
    m_ui->button->setVisible( false );
    m_ui->progressLabel->setVisible( false );
    connect( m_ui->sendButton, SIGNAL( clicked() ), SLOT( onSendButton() ) );

    adjustSize();
    setFixedSize( size() );
}


CrashReporter::~CrashReporter()
{
    delete m_request;
    delete m_reply;
}


void
CrashReporter::setLogo( const QPixmap& logo )
{
    m_ui->logoLabel->setPixmap( logo.scaled( QSize( 55, 55 ), Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
    setWindowIcon( logo );
}

void
CrashReporter::setText( const QString& text )
{
    m_ui->topLabel->setText(text);
}

static QByteArray
contents( const QString& path )
{
    QFile f( path );
    f.open( QFile::ReadOnly );
    return f.readAll();
}


void
CrashReporter::send()
{
    // TODO: check if dump file actually exists ...

    // add minidump file
    setReportData( "upload_file_minidump",
        contents( m_minidump_file_path ),
        "application/octet-stream",
        QFileInfo( m_minidump_file_path ).fileName().toUtf8() );


    QByteArray body;
    foreach (const QByteArray& name, m_formContents.keys() )
    {
        body += "--thkboundary\r\n";

        body += "Content-Disposition: form-data; name=\"" + name + "\"";

        if ( !m_formFileNames.value( name ).isEmpty() && !m_formContentTypes.value( name ).isEmpty() )
        {
            body += "; filename=\"" + m_formFileNames.value( name ) + "\"\r\n";
            body += "Content-Type: " + m_formContentTypes.value( name ) + "\r\n";
        }
        else
        {
            body += "\r\n";
        }

        body += "\r\n";

        body += m_formContents.value( name ) + "\r\n";
    }

    body += "--thkboundary\r\n";


    QNetworkAccessManager* nam = new QNetworkAccessManager( this );
    m_request->setHeader( QNetworkRequest::ContentTypeHeader, "multipart/form-data; boundary=thkboundary" );
    m_reply = nam->post( *m_request, body );

    #if QT_VERSION < QT_VERSION_CHECK( 5, 0, 0 )
    connect( m_reply, SIGNAL( finished() ), SLOT( onDone() ), Qt::QueuedConnection );
    connect( m_reply, SIGNAL( uploadProgress( qint64, qint64 ) ), SLOT( onProgress( qint64, qint64 ) ) );
    #else
    connect( m_reply, &QNetworkReply::finished, this, &CrashReporter::onDone, Qt::QueuedConnection );
    connect( m_reply, &QNetworkReply::uploadProgress, this, &CrashReporter::onProgress );
    #endif
}


void
CrashReporter::onProgress( qint64 done, qint64 total )
{
    if ( total )
    {
        QString const msg = tr( "Uploaded %L1 of %L2 KB." ).arg( done / 1024 ).arg( total / 1024 );

        m_ui->progressBar->setMaximum( total );
        m_ui->progressBar->setValue( done );
        m_ui->progressLabel->setText( msg );
    }
}


void
CrashReporter::onDone()
{
    QByteArray data = m_reply->readAll();
    m_ui->progressBar->setValue( m_ui->progressBar->maximum() );
    m_ui->button->setText( tr( "Close" ) );

    QString const response = QString::fromUtf8( data );

    if ( ( m_reply->error() != QNetworkReply::NoError ) || !response.startsWith( "CrashID=" ) )
    {
        onFail( m_reply->error(), m_reply->errorString() );
    }
    else
        m_ui->progressLabel->setText( tr( "Sent! <b>Many thanks</b>." ) );
}


void
CrashReporter::onFail( int error, const QString& errorString )
{
    m_ui->button->setText( tr( "Close" ) );
    m_ui->progressLabel->setText( tr( "Failed to send crash info." ) );
    qDebug() << "Error:" << error << errorString;
}


void
CrashReporter::onSendButton()
{
    m_ui->progressBar->setVisible( true );
    m_ui->button->setVisible( true );
    m_ui->progressLabel->setVisible( true );
    m_ui->sendButton->setEnabled( false );
    m_ui->dontSendButton->setEnabled( false );

    adjustSize();
    setFixedSize( size() );

    QTimer::singleShot( 0, this, SLOT( send() ) );
}

void
CrashReporter::setReportData(const QByteArray& name, const QByteArray& content)
{
    m_formContents.insert( name, content );
}

void
CrashReporter::setReportData(const QByteArray& name, const QByteArray& content, const QByteArray& contentType, const QByteArray& fileName)
{
    setReportData( name, content );

    if( !contentType.isEmpty() && !fileName.isEmpty() )
    {
        m_formContentTypes.insert( name, contentType );
        m_formFileNames.insert( name, fileName );
    }
}
