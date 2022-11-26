/******************************************************************************\
 * Copyright (c) 2004-2024
 *
 * Author(s):
 *  Volker Fischer
 *  pljones
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
\******************************************************************************/

#include <iostream>
#include <memory>
#include <QCoreApplication>
#ifndef HEADLESS
#    include <QApplication>
#    include <QMessageBox>
#endif
#ifdef ANDROID
#    include <QtAndroidExtras/QtAndroid>
#endif
#if defined( Q_OS_MACX )
extern void qt_set_sequence_auto_mnemonic ( bool bEnable );
#endif

#include "global.h"
#include "util.h"
#include "options.h"
#include "server.h"
#ifndef HEADLESS
#    include "serverdlg.h"
#endif
#ifndef SERVER_ONLY
#    include "client.h"
#    include "testbench.h"
#    ifndef HEADLESS
#        include "clientdlg.h"
#    endif
#endif
#ifndef NO_JSON_RPC
#    include "rpcserver.h"
#    include "serverrpc.h"
#    ifndef SERVER_ONLY
#        include "clientrpc.h"
#    endif
#endif
#if defined( Q_OS_MACX )
#    include "mac/activity.h"
#endif

// Implementation **************************************************************

int main ( int argc, char** argv )
{
#if defined( _WIN32 )
    if ( AttachConsole ( ATTACH_PARENT_PROCESS ) )
    {
        freopen ( "CONOUT$", "w", stdout );
        freopen ( "CONOUT$", "w", stderr );
    }
#endif

#ifdef ANDROID
    // this appears to presume Android isn't a server?
    // request record audio permission
    auto result = QtAndroid::checkPermission ( QString ( "android.permission.RECORD_AUDIO" ) );

    if ( result == QtAndroid::PermissionResult::Denied )
    {
        QtAndroid::PermissionResultMap resultHash = QtAndroid::requestPermissionsSync ( QStringList ( { "android.permission.RECORD_AUDIO" } ) );

        if ( resultHash["android.permission.RECORD_AUDIO"] == QtAndroid::PermissionResult::Denied )
        {
            return 0; // "exit 1" might be better?
        }
    }
#endif

    // parse command line and load settings into allOptions
    QString errmsg;
    if ( !AnyOption::ProcessArgs ( argc, argv, errmsg ) )
    {
        qCritical() << qUtf8Printable ( errmsg );
        exit ( 1 );
    }

    // Application/GUI setup ---------------------------------------------------

    // Application object
    QCoreApplication* pApp =
#ifndef HEADLESS
        !AllOptions.fo_nogui.value ? new QApplication ( argc, argv ) :
#endif
                                   new QCoreApplication ( argc, argv );

    QObject::connect ( pApp, &QCoreApplication::aboutToQuit, AnyOption::SaveSettings );

#if defined( Q_OS_MACX )
#    if !defined( HEADLESS )
    if ( !allOptions.fo_nogui.value )
    {
        // Mnemonic keys are default disabled in Qt for MacOS. The following function enables them.
        // Qt will not show these with underline characters in the GUI on MacOS.
        qt_set_sequence_auto_mnemonic ( true );
    }
#    endif

    // On OSX we need to declare an activity to ensure the process doesn't get
    // throttled by OS level Nap, Sleep, and Thread Priority systems.
    CActivity activity;

    activity.BeginActivity();
#endif

#ifdef _WIN32
    // set application priority class -> high priority
    SetPriorityClass ( GetCurrentProcess(), HIGH_PRIORITY_CLASS ); // REALTIME_PRIORITY_CLASS ?

#    if !defined( HEADLESS )
    // For UI accessible support we need to add a plugin to qt.
    // The plugin has to be located in the install directory of the software by the installer.
    // Here, we set the path to our application path.
    QDir ApplDir ( QApplication::applicationDirPath() );
    pApp->addLibraryPath ( QString ( ApplDir.absolutePath() ) );
#    endif
#endif

    // init resources
    Q_INIT_RESOURCE ( resources );

#ifndef SERVER_ONLY
    //### TEST: BEGIN ###//
    // activate the following line to activate the test bench,
    // CTestbench Testbench ( "127.0.0.1", DEFAULT_PORT_NUMBER );
    //### TEST: END ###//
#endif

    try
    {

#ifndef NO_JSON_RPC
        CRpcServer* pRpcServer = nullptr;
        if ( AllOptions.io_jsonrpcport.value != INVALID_PORT ) // JSON-RPC server enabled
        {
            qWarning() << "- JSON-RPC: This interface is experimental and is subject to breaking changes even on patch versions "
                          "(not subject to semantic versioning) during the initial phase.";

            pRpcServer = new CRpcServer ( pApp );

            if ( !pRpcServer->Start() )
            {
                throw CGenErr ( "Server failed to start.", "JSON-RPC" );
            }
        }
#endif

#ifndef SERVER_ONLY
        if ( !AllOptions.fo_server.value ) // Client
        {
            // actual client object
            CClient Client;

#    ifndef NO_JSON_RPC
            if ( pRpcServer )
            {
                new CClientRpc ( &Client, pRpcServer, pRpcServer );
            }
#    endif

#    ifndef HEADLESS
            if ( !AllOptions.fo_nogui.value )
            {
                // load translation
                if ( !AllOptions.fo_notranslation.value )
                {
                    CLocale::LoadTranslation ( AllOptions.so_language.value, pApp );
                    CInstPictures::UpdateTableOnLanguageChange();
                }

                // GUI object
                CClientDlg ClientDlg ( Client );

                // show dialog
                ClientDlg.show();

                pApp->exec();
            }
            else
#    endif
            {
                pApp->exec();
            }
        }
        else // Server
#endif
        {
            // actual server object
            CServer Server;

#ifndef NO_JSON_RPC
            if ( pRpcServer )
            {
                new CServerRpc ( &Server, pRpcServer, pRpcServer );
            }

#endif
#ifndef HEADLESS
            if ( !AllOptions.fo_nogui.value )
            {
                // load translation
                if ( !AllOptions.fo_notranslation.value )
                {
                    CLocale::LoadTranslation ( AllOptions.so_language.value, pApp );
                }

                // GUI object for the server
                CServerDlg ServerDlg ( Server );

                // show dialog (if the minimized flag is set)
                if ( !AllOptions.fo_startminimized.value )
                {
                    ServerDlg.show();
                }

                pApp->exec();
            }
            else
#endif
            {
                pApp->exec();
            }
        }
    }

    catch ( const CGenErr& generr )
    {
        // show generic error
        qCritical() << qUtf8Printable ( QString ( "%1: %2" ).arg ( APP_NAME ).arg ( generr.GetErrorText() ) );

#ifndef HEADLESS
        if ( !AllOptions.fo_nogui.value )
        {
            QMessageBox::critical ( nullptr, APP_NAME, generr.GetErrorText(), "Quit", nullptr );
        }
        else
#endif
        {
            exit ( 1 );
        }
    }

#if defined( Q_OS_MACX )
    activity.EndActivity();
#endif

    return 0;
}
