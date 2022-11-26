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

#include "settings.h"

/* Implementation *************************************************************/
void CSettings::Load ( const QList<QString>& CommandLineOptions )
// TODO: move the (Get|Set)(|Numeric|Flag)IniSet(ting|) methods into (type...)Option.fromXML(xmlFile, strSection)
// - mostly: read the xml node text, pass that to the validator
// - for vectors: read into a QList<String>, pass that to the validator
// - for use by (type...)Option.fromXML(xmlFile, strSection) and (type...)Option.toXML()
// - Option.fromXML(xmlFile, strSection, strKey, target) - GetIniSetting but bool and takes ref to target
// - Option.toXML(xmlFile, strSection, strKey, sourceAsString) - PutIniSetting
{
    // prepare file name for loading initialization data from XML file and read
    // data from file if possible
    QDomDocument IniXMLDocument;
    ReadFromFile ( strFileName, IniXMLDocument );

    // read the settings from the given XML file
    ReadSettingsFromXML ( IniXMLDocument, CommandLineOptions );
}

void CSettings::Save()
{
    // create XML document for storing initialization parameters
    QDomDocument IniXMLDocument;

    // write the settings in the XML file
    WriteSettingsToXML ( IniXMLDocument );

    // prepare file name for storing initialization data in XML file and store
    // XML data in file
    WriteToFile ( strFileName, IniXMLDocument );
}

void CSettings::ReadFromFile ( const QString& strCurFileName, QDomDocument& XMLDocument )
{
    QFile file ( strCurFileName );

    if ( file.open ( QIODevice::ReadOnly ) )
    {
        XMLDocument.setContent ( QTextStream ( &file ).readAll(), false );
        file.close();
    }
}

void CSettings::WriteToFile ( const QString& strCurFileName, const QDomDocument& XMLDocument )
{
    QFile file ( strCurFileName );

    if ( file.open ( QIODevice::WriteOnly ) )
    {
        QTextStream ( &file ) << XMLDocument.toString();
        file.close();
    }
}

void CSettings::SetFileName ( const QString& sNFiName, const QString& sDefaultFileName )
{
    // return the file name with complete path, take care if given file name is empty
    strFileName = sNFiName;

    if ( strFileName.isEmpty() )
    {
        // we use the Qt default setting file paths for the different OSs by
        // utilizing the QSettings class
        const QString sConfigDir =
            QFileInfo ( QSettings ( QSettings::IniFormat, QSettings::UserScope, APP_NAME, APP_NAME ).fileName() ).absolutePath();

        // make sure the directory exists
        if ( !QFile::exists ( sConfigDir ) )
        {
            QDir().mkpath ( sConfigDir );
        }

        // append the actual file name
        strFileName = sConfigDir + "/" + sDefaultFileName;
    }
}

void CSettings::SetNumericIniSet ( QDomDocument& xmlFile, const QString& strSection, const QString& strKey, const int iValue )
{
    // convert input parameter which is an integer to string and store
    PutIniSetting ( xmlFile, strSection, strKey, QString::number ( iValue ) );
}

bool CSettings::GetNumericIniSet ( const QDomDocument& xmlFile,
                                   const QString&      strSection,
                                   const QString&      strKey,
                                   const int           iRangeStart,
                                   const int           iRangeStop,
                                   int&                iValue )
{
    // init return value
    bool bReturn = false;

    const QString strGetIni = GetIniSetting ( xmlFile, strSection, strKey );

    // check if it is a valid parameter
    if ( !strGetIni.isEmpty() )
    {
        // convert string from init file to integer
        iValue = strGetIni.toInt();

        // check range
        if ( ( iValue >= iRangeStart ) && ( iValue <= iRangeStop ) )
        {
            bReturn = true;
        }
    }

    return bReturn;
}

void CSettings::SetFlagIniSet ( QDomDocument& xmlFile, const QString& strSection, const QString& strKey, const bool bValue )
{
    // we encode true -> "1" and false -> "0"
    PutIniSetting ( xmlFile, strSection, strKey, bValue ? "1" : "0" );
}

bool CSettings::GetFlagIniSet ( const QDomDocument& xmlFile, const QString& strSection, const QString& strKey, bool& bValue )
{
    // init return value
    bool bReturn = false;

    const QString strGetIni = GetIniSetting ( xmlFile, strSection, strKey );

    if ( !strGetIni.isEmpty() )
    {
        bValue  = ( strGetIni.toInt() != 0 );
        bReturn = true;
    }

    return bReturn;
}

// Init-file routines using XML ***********************************************
QString CSettings::GetIniSetting ( const QDomDocument& xmlFile, const QString& sSection, const QString& sKey, const QString& sDefaultVal )
{
    // init return parameter with default value
    QString sResult ( sDefaultVal );

    // get section
    QDomElement xmlSection = xmlFile.firstChildElement ( sSection );

    if ( !xmlSection.isNull() )
    {
        // get key
        QDomElement xmlKey = xmlSection.firstChildElement ( sKey );

        if ( !xmlKey.isNull() )
        {
            // get value
            sResult = xmlKey.text();
        }
    }

    return sResult;
}

void CSettings::PutIniSetting ( QDomDocument& xmlFile, const QString& sSection, const QString& sKey, const QString& sValue )
{
    // check if section is already there, if not then create it
    QDomElement xmlSection = xmlFile.firstChildElement ( sSection );

    if ( xmlSection.isNull() )
    {
        // create new root element and add to document
        xmlSection = xmlFile.createElement ( sSection );
        xmlFile.appendChild ( xmlSection );
    }

    // check if key is already there, if not then create it
    QDomElement xmlKey = xmlSection.firstChildElement ( sKey );

    if ( xmlKey.isNull() )
    {
        xmlKey = xmlFile.createElement ( sKey );
        xmlSection.appendChild ( xmlKey );
    }

    // add actual data to the key
    QDomText currentValue = xmlFile.createTextNode ( sValue );
    xmlKey.appendChild ( currentValue );
}

#ifndef SERVER_ONLY
// Client settings -------------------------------------------------------------
void CClientSettings::LoadFaderSettings ( const QString& strCurFileName )
{
    // prepare file name for loading initialization data from XML file and read
    // data from file if possible
    QDomDocument IniXMLDocument;
    ReadFromFile ( strCurFileName, IniXMLDocument );

    // read the settings from the given XML file
    ReadFaderSettingsFromXML ( IniXMLDocument );
}

void CClientSettings::SaveFaderSettings ( const QString& strCurFileName )
{
    // create XML document for storing initialization parameters
    QDomDocument IniXMLDocument;

    // write the settings in the XML file
    WriteFaderSettingsToXML ( IniXMLDocument );

    // prepare file name for storing initialization data in XML file and store
    // XML data in file
    WriteToFile ( strCurFileName, IniXMLDocument );
}

void CClientSettings::ReadSettingsFromXML ( const QDomDocument& IniXMLDocument, const QList<QString>& )
{
    size_t         iIdx;
    int            iValue;
    bool           bValue;
    QList<QString> slValue = QList<QString>();

    QString successmsg;
    QString errmsg;

    // server addresses
    for ( iIdx = 0; iIdx < MAX_NUM_SERVER_ADDR_ITEMS; iIdx++ )
    {
        slValue += GetIniSetting ( IniXMLDocument, "client", QString ( "ipaddress%1" ).arg ( iIdx ), "" );
    }
    if ( !AllOptions.vo_serveraddresses.validator ( AllOptions.vo_serveraddresses, slValue, successmsg, errmsg ) )
    {
#    ifndef HEADLESS
        if ( !AllOptions.fo_nogui.value )
        {
            QMessageBox::warning ( nullptr, APP_NAME, errmsg );
        }
        else
#    endif
        {
            qCritical() << errmsg;
        }
    }

    // new client level
    if ( GetNumericIniSet ( IniXMLDocument, "client", "newclientlevel", 0, 100, iValue ) )
    {
        AllOptions.io_newclientlevel.value = iValue;
    }

    // input boost
    if ( GetNumericIniSet ( IniXMLDocument, "client", "inputboost", 1, 10, iValue ) )
    {
        AllOptions.io_inputboost.value = iValue;
    }

    if ( GetFlagIniSet ( IniXMLDocument, "client", "enablefeedbackdetection", bValue ) )
    {
        AllOptions.fo_detectfeedback.value = bValue;
    }

    // connect dialog show all musicians
    if ( GetFlagIniSet ( IniXMLDocument, "client", "connectdlgshowallmusicians", bValue ) )
    {
        AllOptions.fo_connectdlgshowallmusicians.value = bValue;
    }

    // language
    AllOptions.so_language.value = GetIniSetting ( IniXMLDocument, "client", "language", AllOptions.so_language.defvalue );

    // fader channel sorting
    if ( GetNumericIniSet ( IniXMLDocument, "client", "channelsort", 0, 4 /* ST_BY_CITY */, iValue ) )
    {
        AllOptions.io_channelsort.value = iValue;
    }

    // own fader first sorting
    if ( GetFlagIniSet ( IniXMLDocument, "client", "ownfaderfirst", bValue ) )
    {
        AllOptions.fo_ownfaderfirst.value = bValue;
    }

    // number of mixer panel rows
    if ( GetNumericIniSet ( IniXMLDocument, "client", "numrowsmixpan", 1, 8, iValue ) )
    {
        AllOptions.io_numrowsmixpan.value = iValue;
    }

    // audio alerts
    if ( GetFlagIniSet ( IniXMLDocument, "client", "enableaudioalerts", bValue ) )
    {
        bEnableAudioAlerts = bValue;
    }

    // name
    AllOptions.so_name.fromBase64String ( GetIniSetting ( IniXMLDocument, "client", "name_base64", AllOptions.so_name.toBase64String() ) );

    // instrument
    if ( GetNumericIniSet ( IniXMLDocument, "client", "instrument", 0, static_cast<int> ( AllOptions.io_instrument.maxvalue ), iValue ) )
    {
        AllOptions.io_instrument.value = iValue;
    }

    // country
    if ( GetNumericIniSet ( IniXMLDocument, "client", "country", 0, static_cast<int> ( AllOptions.io_country.maxvalue ), iValue ) )
    {
        AllOptions.io_country.value = CLocale::WireFormatCountryCodeToQtCountry ( iValue );
    }

    // city
    AllOptions.so_city.fromBase64String ( GetIniSetting ( IniXMLDocument, "client", "city_base64", AllOptions.so_city.toBase64String() ) );

    // skill level
    if ( GetNumericIniSet ( IniXMLDocument, "client", "skill", 0, 3 /* SL_PROFESSIONAL */, iValue ) )
    {
        AllOptions.io_skill.value = iValue;
    }

    // audio fader
    if ( GetNumericIniSet ( IniXMLDocument, "client", "audfad", AUD_FADER_IN_MIN, AUD_FADER_IN_MAX, iValue ) )
    {
        AllOptions.io_audfad.value = iValue;
    }

    // reverberation level
    if ( GetNumericIniSet ( IniXMLDocument, "client", "revlev", 0, AUD_REVERB_MAX, iValue ) )
    {
        AllOptions.io_revlev.value = iValue;
    }

    // reverberation channel assignment
    if ( GetFlagIniSet ( IniXMLDocument, "client", "reverblchan", bValue ) )
    {
        AllOptions.fo_reverblchan.value = bValue;
    }

    // sound card selection
    AllOptions.so_auddev.fromBase64String ( GetIniSetting ( IniXMLDocument, "client", "auddev_base64", AllOptions.so_auddev.toBase64String() ) );

    // sound card channel mapping settings: make sure these settings are
    // set AFTER the sound card device is set, otherwise the settings are
    // overwritten by the defaults
    //
    // sound card left input channel mapping
    if ( GetNumericIniSet ( IniXMLDocument, "client", "sndcrdinlch", 0, MAX_NUM_IN_OUT_CHANNELS - 1, iValue ) )
    {
        AllOptions.io_sndcrdinlch.value = iValue;
    }

    // sound card right input channel mapping
    if ( GetNumericIniSet ( IniXMLDocument, "client", "sndcrdinrch", 0, MAX_NUM_IN_OUT_CHANNELS - 1, iValue ) )
    {
        AllOptions.io_sndcrdinrch.value = iValue;
    }

    // sound card left output channel mapping
    if ( GetNumericIniSet ( IniXMLDocument, "client", "sndcrdoutlch", 0, MAX_NUM_IN_OUT_CHANNELS - 1, iValue ) )
    {
        AllOptions.io_sndcrdoutlch.value = iValue;
    }

    // sound card right output channel mapping
    if ( GetNumericIniSet ( IniXMLDocument, "client", "sndcrdoutrch", 0, MAX_NUM_IN_OUT_CHANNELS - 1, iValue ) )
    {
        AllOptions.io_sndcrdoutrch.value = iValue;
    }

    // sound card preferred buffer size index
    if ( GetNumericIniSet ( IniXMLDocument, "client", "prefsndcrdbufidx", FRAME_SIZE_FACTOR_PREFERRED, FRAME_SIZE_FACTOR_SAFE, iValue ) )
    {
        // additional check required since only a subset of factors are defined
        if ( ( iValue == FRAME_SIZE_FACTOR_PREFERRED ) || ( iValue == FRAME_SIZE_FACTOR_DEFAULT ) || ( iValue == FRAME_SIZE_FACTOR_SAFE ) )
        {
            AllOptions.io_prefsndcrdbufidx.value = iValue;
        }
    }

    // automatic network jitter buffer size setting
    if ( GetFlagIniSet ( IniXMLDocument, "client", "autojitbuf", bValue ) )
    {
        AllOptions.fo_autojitbuf.value = bValue;
    }

    // network jitter buffer size
    if ( GetNumericIniSet ( IniXMLDocument, "client", "jitbuf", MIN_NET_BUF_SIZE_NUM_BL, MAX_NET_BUF_SIZE_NUM_BL, iValue ) )
    {
        AllOptions.io_jitbuf.value = iValue;
    }

    // network jitter buffer size for server
    if ( GetNumericIniSet ( IniXMLDocument, "client", "jitbufserver", MIN_NET_BUF_SIZE_NUM_BL, MAX_NET_BUF_SIZE_NUM_BL, iValue ) )
    {
        AllOptions.io_jitbufserver.value = iValue;
    }

    // enable OPUS64 setting
    if ( GetFlagIniSet ( IniXMLDocument, "client", "enableopussmall", bValue ) )
    {
        AllOptions.fo_enableopussmall.value = bValue;
    }

    // GUI design
    if ( GetNumericIniSet ( IniXMLDocument, "client", "guidesign", 0, 2 /* GD_SLIMFADER */, iValue ) )
    {
        AllOptions.io_guidesign.value = iValue;
    }

    // MeterStyle
    if ( GetNumericIniSet ( IniXMLDocument, "client", "meterstyle", 0, 4 /* MT_LED_ROUND_BIG */, iValue ) )
    {
        AllOptions.io_meterstyle.value = iValue;
    }
    else
    {
        // if MeterStyle is not found in the ini, set it based on the GUI design
        if ( GetNumericIniSet ( IniXMLDocument, "client", "guidesign", 0, 2 /* GD_SLIMFADER */, iValue ) )
        {
            switch ( iValue )
            {
            case GD_STANDARD:
                AllOptions.io_meterstyle.value = MT_BAR_WIDE;
                break;

            case GD_ORIGINAL:
                AllOptions.io_meterstyle.value = MT_LED_STRIPE;
                break;

            case GD_SLIMFADER:
                AllOptions.io_meterstyle.value = MT_BAR_NARROW;
                break;

            default:
                AllOptions.io_meterstyle.value = MT_LED_STRIPE;
                break;
            }
        }
    }

    // audio channels
    if ( GetNumericIniSet ( IniXMLDocument, "client", "audiochannels", 0, 2 /* CC_STEREO */, iValue ) )
    {
        AllOptions.io_audiochannels.value = iValue;
    }

    // audio quality
    if ( GetNumericIniSet ( IniXMLDocument, "client", "audioquality", 0, 2 /* AQ_HIGH */, iValue ) )
    {
        AllOptions.io_audioquality.value = iValue;
    }

    slValue = QList<QString>();
    // custom directories

    //### TODO: BEGIN ###//
    // compatibility to old version (< 3.6.1)
    QString strDirectoryAddress = GetIniSetting ( IniXMLDocument, "client", "centralservaddr", "" );
    //### TODO: END ###//

    for ( iIdx = 0; iIdx < MAX_NUM_SERVER_ADDR_ITEMS; iIdx++ )
    {
        //### TODO: BEGIN ###//
        // compatibility to old version (< 3.8.2)
        strDirectoryAddress = GetIniSetting ( IniXMLDocument, "client", QString ( "centralservaddr%1" ).arg ( iIdx ), strDirectoryAddress );
        //### TODO: END ###//
        strDirectoryAddress = GetIniSetting ( IniXMLDocument, "client", QString ( "directoryaddress%1" ).arg ( iIdx ), strDirectoryAddress );
        if ( !strDirectoryAddress.isEmpty() )
        {
            slValue += strDirectoryAddress;
        }
        strDirectoryAddress = "";
    }
    if ( !AllOptions.vo_directoryaddresses.validator ( AllOptions.vo_directoryaddresses, slValue, successmsg, errmsg ) )
    {
#    ifndef HEADLESS
        if ( !AllOptions.fo_nogui.value )
        {
            QMessageBox::warning ( nullptr, APP_NAME, errmsg );
        }
        else
#    endif
        {
            qCritical() << errmsg;
        }
    }

    // directory type
    // allOptions.io_directorytype.defvalue is AT_DEFAULT - if nothing in settings, that remains correct
    //### TODO: BEGIN ###//
    // compatibility to old version (<3.4.7)
    // only the case that "centralservaddr" was set in old ini must be considered (otherwise we have no custom addr)
    if ( !AllOptions.vo_directoryaddresses.value[0].isEmpty() && GetFlagIniSet ( IniXMLDocument, "client", "defcentservaddr", bValue ) && !bValue )
    {
        AllOptions.io_directorytype.value = AT_CUSTOM;
    }
    // compatibility to old version (< 3.8.2)
    // regardless of any defcentserveraddr, if we find centservaddrtype use that instead
    if ( GetNumericIniSet ( IniXMLDocument, "client", "centservaddrtype", 0, static_cast<int> ( AT_CUSTOM ), iValue ) )
    {
        AllOptions.io_directorytype.value = static_cast<EDirectoryType> ( iValue );
    }
    //### TODO: END ###//
    if ( GetNumericIniSet ( IniXMLDocument, "client", "directorytype", 0, static_cast<int> ( AT_CUSTOM ), iValue ) )
    {
        AllOptions.io_directorytype.value = static_cast<EDirectoryType> ( iValue );
    }

    // custom directory index in the connect UI - must be zero (default) if not custom to work correctly
    if ( ( AllOptions.io_directorytype.value == AT_CUSTOM ) &&
         GetNumericIniSet ( IniXMLDocument, "client", "customdirectoryindex", 0, MAX_NUM_SERVER_ADDR_ITEMS, iValue ) )
    {
        AllOptions.io_customdirectoryindex.value = iValue;
    }

    // window position of the main window
    AllOptions.bo_winposmain.fromBase64String ( GetIniSetting ( IniXMLDocument, "client", "winposmain_base64" ) );

    // window position of the settings window
    AllOptions.bo_winposset.fromBase64String ( GetIniSetting ( IniXMLDocument, "client", "winposset_base64" ) );

    // window position of the chat window
    AllOptions.bo_winposchat.fromBase64String ( GetIniSetting ( IniXMLDocument, "client", "winposchat_base64" ) );

    // window position of the connect window
    AllOptions.bo_winposcon.fromBase64String ( GetIniSetting ( IniXMLDocument, "client", "winposcon_base64" ) );

    // visibility state of the settings window
    if ( GetFlagIniSet ( IniXMLDocument, "client", "winvisset", bValue ) )
    {
        AllOptions.fo_winvisset.value = bValue;
    }

    // visibility state of the chat window
    if ( GetFlagIniSet ( IniXMLDocument, "client", "winvischat", bValue ) )
    {
        AllOptions.fo_winvischat.value = bValue;
    }

    // visibility state of the connect window
    if ( GetFlagIniSet ( IniXMLDocument, "client", "winviscon", bValue ) )
    {
        AllOptions.fo_winviscon.value = bValue;
    }

    // selected Settings Tab
    if ( GetNumericIniSet ( IniXMLDocument, "client", "settingstab", 0, 2, iValue ) )
    {
        AllOptions.io_settingstab.value = iValue;
    }

    // fader settings
    ReadFaderSettingsFromXML ( IniXMLDocument );
}

void CClientSettings::ReadFaderSettingsFromXML ( const QDomDocument& IniXMLDocument )
{
    uint iIdx;
    int  iValue;
    bool bValue;

    for ( iIdx = 0; iIdx < MAX_NUM_STORED_FADER_SETTINGS; iIdx++ )
    {
        // stored fader tags
        AllOptions.vo_storedfadertag.value[iIdx] =
            FromBase64 ( GetIniSetting ( IniXMLDocument, "client", QString ( "storedfadertag%1_base64" ).arg ( iIdx ), "" ) );

        // stored fader levels
        if ( GetNumericIniSet ( IniXMLDocument, "client", QString ( "storedfaderlevel%1" ).arg ( iIdx ), 0, AUD_MIX_FADER_MAX, iValue ) )
        {
            AllOptions.vo_storedfaderlevel.value[iIdx] = iValue;
        }

        // stored pan values
        if ( GetNumericIniSet ( IniXMLDocument, "client", QString ( "storedpanvalue%1" ).arg ( iIdx ), 0, AUD_MIX_PAN_MAX, iValue ) )
        {
            AllOptions.vo_storedpanvalue.value[iIdx] = iValue;
        }

        // stored fader solo state
        if ( GetFlagIniSet ( IniXMLDocument, "client", QString ( "storedfaderissolo%1" ).arg ( iIdx ), bValue ) )
        {
            AllOptions.vo_storedfaderissolo.value[iIdx] = bValue;
        }

        // stored fader muted state
        if ( GetFlagIniSet ( IniXMLDocument, "client", QString ( "storedfaderismute%1" ).arg ( iIdx ), bValue ) )
        {
            AllOptions.vo_storedfaderismute.value[iIdx] = bValue;
        }

        // stored fader group ID
        if ( GetNumericIniSet ( IniXMLDocument,
                                "client",
                                QString ( "storedgroupid%1" ).arg ( iIdx ),
                                INVALID_INDEX,
                                MAX_NUM_FADER_GROUPS - 1,
                                iValue ) )
        {
            AllOptions.vo_storedgroupid.value[iIdx] = iValue;
        }
    }
}

void CClientSettings::WriteSettingsToXML ( QDomDocument& IniXMLDocument )
{
    size_t iIdx;

    // IP addresses
    for ( iIdx = 0; iIdx < MAX_NUM_SERVER_ADDR_ITEMS; iIdx++ )
    {
        PutIniSetting ( IniXMLDocument, "client", QString ( "ipaddress%1" ).arg ( iIdx ), AllOptions.vo_serveraddresses.value[iIdx] );
    }

    // new client level
    SetNumericIniSet ( IniXMLDocument, "client", "newclientlevel", AllOptions.io_newclientlevel.toInt() );

    // input boost
    SetNumericIniSet ( IniXMLDocument, "client", "inputboost", AllOptions.io_inputboost.toInt() );

    // feedback detection
    SetFlagIniSet ( IniXMLDocument, "client", "enablefeedbackdetection", AllOptions.fo_detectfeedback.value );

    // connect dialog show all musicians
    SetFlagIniSet ( IniXMLDocument, "client", "connectdlgshowallmusicians", AllOptions.fo_connectdlgshowallmusicians.value );

    // language
    PutIniSetting ( IniXMLDocument, "client", "language", AllOptions.so_language.value );

    // fader channel sorting
    SetNumericIniSet ( IniXMLDocument, "client", "channelsort", AllOptions.io_channelsort.toInt() );

    // own fader first sorting
    SetFlagIniSet ( IniXMLDocument, "client", "ownfaderfirst", AllOptions.fo_ownfaderfirst.value );

    // number of mixer panel rows
    SetNumericIniSet ( IniXMLDocument, "client", "numrowsmixpan", AllOptions.io_numrowsmixpan.toInt() );

    // audio alerts
    SetFlagIniSet ( IniXMLDocument, "client", "enableaudioalerts", bEnableAudioAlerts );

    // name
    PutIniSetting ( IniXMLDocument, "client", "name_base64", AllOptions.so_name.toBase64String() );

    // instrument
    SetNumericIniSet ( IniXMLDocument, "client", "instrument", AllOptions.io_instrument.toInt() );

    // country
    SetNumericIniSet (
        IniXMLDocument,
        "client",
        "country",
        static_cast<int> ( CLocale::QtCountryToWireFormatCountryCode ( static_cast<QLocale::Country> ( AllOptions.io_country.value ) ) ) );

    // city
    PutIniSetting ( IniXMLDocument, "client", "city_base64", AllOptions.so_city.toBase64String() );

    // skill level
    SetNumericIniSet ( IniXMLDocument, "client", "skill", AllOptions.io_skill.toInt() );

    // audio fader
    SetNumericIniSet ( IniXMLDocument, "client", "audfad", AllOptions.io_audfad.toInt() );

    // reverberation level
    SetNumericIniSet ( IniXMLDocument, "client", "revlev", AllOptions.io_revlev.toInt() );

    // reverberation channel assignment
    SetFlagIniSet ( IniXMLDocument, "client", "reverblchan", AllOptions.fo_reverblchan.value );

    // sound card selection
    PutIniSetting ( IniXMLDocument, "client", "auddev_base64", AllOptions.so_auddev.toBase64String() );

    // sound card left input channel mapping
    SetNumericIniSet ( IniXMLDocument, "client", "sndcrdinlch", AllOptions.io_sndcrdinlch.toInt() );

    // sound card right input channel mapping
    SetNumericIniSet ( IniXMLDocument, "client", "sndcrdinrch", AllOptions.io_sndcrdinrch.toInt() );

    // sound card left output channel mapping
    SetNumericIniSet ( IniXMLDocument, "client", "sndcrdoutlch", AllOptions.io_sndcrdoutlch.toInt() );

    // sound card right output channel mapping
    SetNumericIniSet ( IniXMLDocument, "client", "sndcrdoutrch", AllOptions.io_sndcrdoutrch.toInt() );

    // sound card preferred buffer size index
    SetNumericIniSet ( IniXMLDocument, "client", "prefsndcrdbufidx", AllOptions.io_prefsndcrdbufidx.toInt() );

    // automatic network jitter buffer size setting
    SetFlagIniSet ( IniXMLDocument, "client", "autojitbuf", AllOptions.fo_autojitbuf.value );

    // network jitter buffer size
    SetNumericIniSet ( IniXMLDocument, "client", "jitbuf", AllOptions.io_sndcrdinlch.toInt() );

    // network jitter buffer size for server
    SetNumericIniSet ( IniXMLDocument, "client", "jitbufserver", AllOptions.io_sndcrdinlch.toInt() );

    // enable OPUS64 setting
    SetFlagIniSet ( IniXMLDocument, "client", "enableopussmall", AllOptions.fo_enableopussmall.value );

    // GUI design
    SetNumericIniSet ( IniXMLDocument, "client", "guidesign", AllOptions.io_guidesign.toInt() );

    // MeterStyle
    SetNumericIniSet ( IniXMLDocument, "client", "meterstyle", AllOptions.io_meterstyle.toInt() );

    // audio channels
    SetNumericIniSet ( IniXMLDocument, "client", "audiochannels", AllOptions.io_audiochannels.toInt() );

    // audio quality
    SetNumericIniSet ( IniXMLDocument, "client", "audioquality", AllOptions.io_audioquality.toInt() );

    // custom directories
    for ( iIdx = 0; iIdx < MAX_NUM_SERVER_ADDR_ITEMS; iIdx++ )
    {
        PutIniSetting ( IniXMLDocument, "client", QString ( "directoryaddress%1" ).arg ( iIdx ), AllOptions.vo_directoryaddresses.value[iIdx] );
    }

    // directory type
    SetNumericIniSet ( IniXMLDocument, "client", "directorytype", AllOptions.io_directorytype.toInt() );

    // custom directory index
    SetNumericIniSet ( IniXMLDocument, "client", "customdirectoryindex", AllOptions.io_customdirectoryindex.toInt() );

    // window position of the main window
    PutIniSetting ( IniXMLDocument, "client", "winposmain_base64", AllOptions.bo_winposmain.toBase64String() );

    // window position of the settings window
    PutIniSetting ( IniXMLDocument, "client", "winposset_base64", AllOptions.bo_winposset.toBase64String() );

    // window position of the chat window
    PutIniSetting ( IniXMLDocument, "client", "winposchat_base64", AllOptions.bo_winposchat.toBase64String() );

    // window position of the connect window
    PutIniSetting ( IniXMLDocument, "client", "winposcon_base64", AllOptions.bo_winposcon.toBase64String() );

    // visibility state of the settings window
    SetFlagIniSet ( IniXMLDocument, "client", "winvisset", AllOptions.fo_winvisset.value );

    // visibility state of the chat window
    SetFlagIniSet ( IniXMLDocument, "client", "winvischat", AllOptions.fo_winvischat.value );

    // visibility state of the connect window
    SetFlagIniSet ( IniXMLDocument, "client", "winviscon", AllOptions.fo_winviscon.value );

    // Settings Tab
    SetNumericIniSet ( IniXMLDocument, "client", "settingstab", AllOptions.io_settingstab.toInt() );

    // fader settings
    WriteFaderSettingsToXML ( IniXMLDocument );
}

void CClientSettings::WriteFaderSettingsToXML ( QDomDocument& IniXMLDocument )
{
    size_t iIdx;

    for ( iIdx = 0; iIdx < MAX_NUM_STORED_FADER_SETTINGS; iIdx++ )
    {
        // stored fader tags
        PutIniSetting ( IniXMLDocument,
                        "client",
                        QString ( "storedfadertag%1_base64" ).arg ( iIdx ),
                        ToBase64 ( AllOptions.vo_storedfadertag.value[iIdx] ) );

        // stored fader levels
        SetNumericIniSet ( IniXMLDocument, "client", QString ( "storedfaderlevel%1" ).arg ( iIdx ), AllOptions.vo_storedfaderlevel.value[iIdx] );

        // stored pan values
        SetNumericIniSet ( IniXMLDocument, "client", QString ( "storedpanvalue%1" ).arg ( iIdx ), AllOptions.vo_storedpanvalue.value[iIdx] );

        // stored fader solo states
        SetFlagIniSet ( IniXMLDocument, "client", QString ( "storedfaderissolo%1" ).arg ( iIdx ), AllOptions.vo_storedfaderissolo.value[iIdx] != 0 );

        // stored fader muted states
        SetFlagIniSet ( IniXMLDocument, "client", QString ( "storedfaderismute%1" ).arg ( iIdx ), AllOptions.vo_storedfaderismute.value[iIdx] != 0 );

        // stored fader group ID
        SetNumericIniSet ( IniXMLDocument, "client", QString ( "storedgroupid%1" ).arg ( iIdx ), AllOptions.vo_storedgroupid.value[iIdx] );
    }
}
#endif

// Server settings -------------------------------------------------------------
// that this gets called means we are not headless
void CServerSettings::ReadSettingsFromXML ( const QDomDocument& IniXMLDocument, const QList<QString>& )
{
    int  iValue;
    bool bValue;

    // window position of the main window
    AllOptions.bo_winposmain.fromBase64String ( GetIniSetting ( IniXMLDocument, "server", "winposmain_base64" ) );

    // name
    AllOptions.so_name.value = GetIniSetting ( IniXMLDocument, "server", "name", AllOptions.so_name.defvalue );
    AllOptions.so_name.fromBase64String ( GetIniSetting ( IniXMLDocument, "server", "name_base64", AllOptions.so_name.toBase64String() ) );

    // city
    AllOptions.so_city.value = GetIniSetting ( IniXMLDocument, "server", "city", AllOptions.so_city.defvalue );
    AllOptions.so_city.fromBase64String ( GetIniSetting ( IniXMLDocument, "server", "city_base64", AllOptions.so_city.toBase64String() ) );

    // country
    if ( GetNumericIniSet ( IniXMLDocument, "server", "country", 0, static_cast<int> ( AllOptions.io_country.maxvalue ), iValue ) )
    {
        AllOptions.io_country.value = CLocale::WireFormatCountryCodeToQtCountry ( iValue );
    }

    // norecord flag
    if ( GetFlagIniSet ( IniXMLDocument, "server", "norecord", bValue ) )
    {
        AllOptions.fo_norecord.value = bValue;
    }

    // welcome message
    AllOptions.so_welcomemessage.fromBase64String (
        GetIniSetting ( IniXMLDocument, "server", "welcome", AllOptions.so_welcomemessage.toBase64String() ) );

    // language
    AllOptions.so_language.value = GetIniSetting ( IniXMLDocument, "server", "language", AllOptions.so_language.defvalue );

    // base recording directory
    AllOptions.so_recording.fromBase64String (
        GetIniSetting ( IniXMLDocument, "server", "recordingdir_base64", AllOptions.so_recording.toBase64String() ) );

    // custom directory address
    //### TODO: BEGIN ###//
    // compatibility to old version < 3.8.2
    AllOptions.so_directory.value = GetIniSetting ( IniXMLDocument, "server", "centralservaddr", AllOptions.so_directory.defvalue );
    // compatibility to old version < x.y.z
    AllOptions.so_directory.value = GetIniSetting ( IniXMLDocument, "server", "directoryaddress", AllOptions.so_directory.value );
    //### TODO: END ###//
    AllOptions.so_directory.fromBase64String (
        GetIniSetting ( IniXMLDocument, "server", "directoryaddress_base64", AllOptions.so_directory.toBase64String() ) );

    // directory type
    if ( GetNumericIniSet ( IniXMLDocument, "server", "country", 0, static_cast<int> ( AllOptions.io_country.maxvalue ), iValue ) )
    {
        AllOptions.io_country.value = iValue;
    }

    // directory type
    // io_directorytype defaults to AT_DEFAULT but for the server we want AT_NONE as the default
    AllOptions.io_directorytype.value = AT_NONE;

    //### TODO: BEGIN ###//
    // compatibility to old version (<3.4.7)
    // only the case that "centralservaddr" was set in old ini must be considered (otherwise we have no custom addr)
    if ( !AllOptions.so_directory.value.isEmpty() && GetFlagIniSet ( IniXMLDocument, "server", "defcentservaddr", bValue ) && !bValue )
    {
        AllOptions.io_directorytype.value = AT_CUSTOM;
    }
    // compatibility to old version < 3.9.0
    // if servlistenabled is still present but false, leave directorytype as AT_NONE
    if ( !GetFlagIniSet ( IniXMLDocument, "server", "servlistenabled", bValue ) || bValue )
    {
        // compatibility to old version (< 3.8.2)
        // regardless of any defcentserveraddr, if we find centservaddrtype use that instead
        if ( GetNumericIniSet ( IniXMLDocument, "server", "centservaddrtype", 0, static_cast<int> ( AT_CUSTOM ), iValue ) )
        {
            directoryType = bValue ? AT_DEFAULT : AT_CUSTOM;
        }
        else
            //### TODO: END ###//

            // if "directorytype" itself is set, use it (note "AT_NONE", "AT_DEFAULT" and "AT_CUSTOM" are min/max directory type here)

            //### TODO: BEGIN ###//
            // compatibility to old version < 3.8.2
            if ( GetNumericIniSet ( IniXMLDocument,
                                    "server",
                                    "centservaddrtype",
                                    static_cast<int> ( AT_DEFAULT ),
                                    static_cast<int> ( AT_CUSTOM ),
                                    iValue ) )
            {
                directoryType = static_cast<EDirectoryType> ( iValue );
            }
            //### TODO: END ###//

            else if ( GetNumericIniSet ( IniXMLDocument,
                                         "server",
                                         "directorytype",
                                         static_cast<int> ( AT_NONE ),
                                         static_cast<int> ( AT_CUSTOM ),
                                         iValue ) )
            {
                directoryType = static_cast<EDirectoryType> ( iValue );
            }

        // server list persistence file name
        AllOptions.so_serverlistfile.fromBase64String (
            GetIniSetting ( IniXMLDocument, "server", "directoryfile_base64", AllOptions.so_serverlistfile.toBase64String() ) );

        // start minimized on OS start
        if ( GetFlagIniSet ( IniXMLDocument, "server", "autostartmin", bValue ) )
        {
            AllOptions.fo_startminimized.value = bValue;
        }

        // delay panning
        if ( GetFlagIniSet ( IniXMLDocument, "server", "delaypan", bValue ) )
        {
            AllOptions.fo_delaypan.value = bValue;
        }
    }
}

void CServerSettings::WriteSettingsToXML ( QDomDocument& IniXMLDocument )
{
    // window position of the main window
    PutIniSetting ( IniXMLDocument, "server", "winposmain_base64", AllOptions.bo_winposmain.toBase64String() );

    // directory type
    SetNumericIniSet ( IniXMLDocument, "server", "directorytype", AllOptions.io_directorytype.toInt() );

    // name
    PutIniSetting ( IniXMLDocument, "server", "name_base64", AllOptions.so_name.toBase64String() );

    // city
    PutIniSetting ( IniXMLDocument, "server", "city_base64", AllOptions.so_city.toBase64String() );

    // country
    SetNumericIniSet ( IniXMLDocument,
                       "server",
                       "country",
                       CLocale::QtCountryToWireFormatCountryCode ( static_cast<QLocale::Country> ( AllOptions.io_country.value ) ) );

    // norecord flag
    SetFlagIniSet ( IniXMLDocument, "server", "norecord", AllOptions.fo_norecord.value );

    // welcome message
    PutIniSetting ( IniXMLDocument, "server", "welcome", AllOptions.so_welcomemessage.toBase64String() );

    // language
    PutIniSetting ( IniXMLDocument, "server", "language", AllOptions.so_language.value );

    // base recording directory
    PutIniSetting ( IniXMLDocument, "server", "recordingdir_base64", AllOptions.so_recording.toBase64String() );

    // custom directory
    PutIniSetting ( IniXMLDocument, "server", "directoryaddress", AllOptions.so_directory.value );

    // server list persistence file name
    PutIniSetting ( IniXMLDocument, "server", "directoryfile_base64", AllOptions.so_serverlistfile.toBase64String() );

    // start minimized on OS start
    SetFlagIniSet ( IniXMLDocument, "server", "autostartmin", AllOptions.fo_startminimized.value );

    // delay panning
    SetFlagIniSet ( IniXMLDocument, "server", "delaypan", AllOptions.fo_delaypan.value );
}
