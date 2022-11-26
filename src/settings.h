/******************************************************************************\
 * Copyright (c) 2004-2024
 *
 * Author(s):
 *  Volker Fischer
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

#pragma once

#include <QDomDocument>
#include <QFile>
#include <QSettings>
#include <QDir>
#ifndef HEADLESS
#    include <QMessageBox>
#endif
#include "global.h"
#ifndef SERVER_ONLY
#    include "client.h"
#endif
#include "server.h"
#include "util.h"

/* Classes ********************************************************************/
class CSettings : public QObject
{
    Q_OBJECT

public:
    CSettings() { QObject::connect ( QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &CSettings::OnAboutToQuit ); }

    void Load ( const QList<QString>& CommandLineOptions );
    void Save();

protected:
    virtual void WriteSettingsToXML ( QDomDocument& IniXMLDocument )                                                  = 0;
    virtual void ReadSettingsFromXML ( const QDomDocument& IniXMLDocument, const QList<QString>& CommandLineOptions ) = 0;

    void ReadFromFile ( const QString& strCurFileName, QDomDocument& XMLDocument );

    void WriteToFile ( const QString& strCurFileName, const QDomDocument& XMLDocument );

    void SetFileName ( const QString& sNFiName, const QString& sDefaultFileName );

    // The following functions implement the conversion from the general string
    // to base64 (which should be used for binary data in XML files). This
    // enables arbitrary utf8 characters to be used as the names in the GUI.
    // (Only needed for VectorOption<QString>.)
    QString ToBase64 ( const QString strIn ) const { return QString::fromLatin1 ( strIn.toUtf8().toBase64() ); }
    QString FromBase64 ( const QString strIn ) const { return QString::fromUtf8 ( QByteArray::fromBase64 ( strIn.toLatin1() ) ); }

    // init file access function for read/write
    void SetNumericIniSet ( QDomDocument& xmlFile, const QString& strSection, const QString& strKey, const int iValue = 0 );

    bool GetNumericIniSet ( const QDomDocument& xmlFile,
                            const QString&      strSection,
                            const QString&      strKey,
                            const int           iRangeStart,
                            const int           iRangeStop,
                            int&                iValue );

    void SetFlagIniSet ( QDomDocument& xmlFile, const QString& strSection, const QString& strKey, const bool bValue = false );

    bool GetFlagIniSet ( const QDomDocument& xmlFile, const QString& strSection, const QString& strKey, bool& bValue );

    // actual working function for init-file access
    QString GetIniSetting ( const QDomDocument& xmlFile, const QString& sSection, const QString& sKey, const QString& sDefaultVal = "" );

    void PutIniSetting ( QDomDocument& xmlFile, const QString& sSection, const QString& sKey, const QString& sValue = "" );

    QString strFileName = "";

public slots:
    void OnAboutToQuit() { Save(); }
};

#ifndef SERVER_ONLY
class CClientSettings : public CSettings
{
public:
    CClientSettings ( const QString& sNFiName ) : CSettings() { SetFileName ( sNFiName, DEFAULT_INI_FILE_NAME ); }

    void LoadFaderSettings ( const QString& strCurFileName );
    void SaveFaderSettings ( const QString& strCurFileName );

protected:
    // No CommandLineOptions used when reading Client inifile
    virtual void WriteSettingsToXML ( QDomDocument& IniXMLDocument ) override;
    virtual void ReadSettingsFromXML ( const QDomDocument& IniXMLDocument, const QList<QString>& ) override;

    void ReadFaderSettingsFromXML ( const QDomDocument& IniXMLDocument );
    void WriteFaderSettingsToXML ( QDomDocument& IniXMLDocument );
};
#endif

class CServerSettings : public CSettings
{
public:
    CServerSettings ( const QString& sNFiName ) : CSettings() { SetFileName ( sNFiName, DEFAULT_INI_FILE_NAME_SERVER ); }

protected:
    virtual void WriteSettingsToXML ( QDomDocument& IniXMLDocument ) override;
    virtual void ReadSettingsFromXML ( const QDomDocument& IniXMLDocument, const QList<QString>& ) override;
};
