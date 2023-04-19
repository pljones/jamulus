/******************************************************************************\
 * Copyright (c) 2021, 2022
 *
 * Author(s):
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

#include "options.h"

bool StringOption::allValid ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg )
{
    if ( args.size() == 0 )
    {
        errmsg = "Missing arg";
        return false;
    }
    ref.value  = args.takeFirst();
    successmsg = ref.validText.contains ( "%1" ) ? ref.validText.arg ( ref.value ) : ref.validText;
    return true;
}

bool StringOption::validFile ( StringOption&  ref,
                               QList<QString> args,
                               QString&       successmsg,
                               QString&       errmsg,
                               QString        errmsgWrapper,
                               bool           readOnly,
                               bool           mustExist,
                               bool           loadAsValue )
{
    ref.value = args.takeFirst();
    if ( !ref.validFile ( errmsg, readOnly, mustExist, loadAsValue ) )
    {
        errmsg    = QString ( errmsgWrapper ).arg ( errmsg );
        ref.value = QString();
        return false;
    }
    successmsg = ref.validText.contains ( "%1" ) ? ref.validText.arg ( ref.value ) : ref.validText;
    return true;
}

bool StringOption::validFile ( QString& errmsg, bool readOnly, bool mustExist, bool loadAsValue )
{
    if ( value.isEmpty() )
    {
        errmsg = "please provide a file name";
        return false;
    }

    QFileInfo fileInfo ( value );
    if ( fileInfo.exists() )
    {
        if ( !fileInfo.isFile() )
        {
            errmsg = "please supply a plain file";
            return false;
        }
        if ( !fileInfo.isReadable() )
        {
            errmsg = "the file must be readable - please check the permissions";
            return false;
        }
        if ( !readOnly && !fileInfo.isWritable() )
        {
            errmsg = "the file must be writeable - please check the permissions";
            return false;
        }
        if ( loadAsValue )
        {
            QFile file ( value );
            if ( !file.open ( QFile::OpenModeFlag::ReadOnly ) )
            {
                errmsg = QString ( "cannot read file" );
                return false;
            }
            QTextStream textStream ( &file );
            value = textStream.readAll();
        }
    }
    else if ( mustExist )
    {
        errmsg = "please supply an existing file";
        return false;
    }
    else
    {
        QFileInfo pathFileInfo ( fileInfo.absolutePath() );
        if ( !pathFileInfo.isDir() )
        {
            errmsg = "the path for the file is not valid";
            return false;
        }
        if ( !pathFileInfo.isReadable() || !pathFileInfo.isWritable() )
        {
            errmsg = "the path for file must be readable and writeable - please check the permissions";
            return false;
        }
    }

    return true;
}

bool StringOption::validDir ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg, QString errmsgWrapper, bool mustExist )
{
    ref.value = args.takeFirst();
    if ( !ref.validDir ( errmsg, mustExist ) )
    {
        errmsg    = QString ( errmsgWrapper ).arg ( errmsg );
        ref.value = QString();
        return false;
    }
    successmsg = ref.validText.contains ( "%1" ) ? ref.validText.arg ( ref.value ) : ref.validText;
    return true;
}

bool StringOption::validDir ( QString& errmsg, bool mustExist )
{
    if ( value.isEmpty() )
    {
        errmsg = "please provide a path name";
        return false;
    }
    QFileInfo pathFileInfo ( value );
    if ( pathFileInfo.exists() )
    {
        if ( !pathFileInfo.isDir() )
        {
            errmsg = "the name supplied is not a valid path";
            return false;
        }
        if ( !pathFileInfo.isReadable() || !pathFileInfo.isWritable() )
        {
            errmsg = "the path must be readable and writeable - please check the permissions";
            return false;
        }
    }
    else if ( mustExist )
    {
        errmsg = "please supply an existing path";
        return false;
    }
    else
    {
        QFileInfo parentFileInfo ( pathFileInfo.absolutePath() );
        if ( !parentFileInfo.isDir() )
        {
            errmsg = "the parent of the name supplied is not a valid path";
            return false;
        }
        if ( !parentFileInfo.isReadable() || !parentFileInfo.isWritable() )
        {
            errmsg = "the parent path must be readable and writeable - please check the permissions";
            return false;
        }
    }
    return true;
}

StringOption& StringOption::operator= ( StringOption rhs )
{
    TOption<QString>::operator= ( rhs );
    validator = rhs.validator;
    return *this;
}

bool StringOption::readXmlDefault ( const QDomElement& xmlSection, QString& errmsg, StringOption::StringOptionValidator validatorX )
{
    QString xmlText;
    if ( !TOption<QString>::readXmlDefault ( *this, xmlSection, xmlText, errmsg ) )
    {
        return false;
    }
    if ( xmlText == QString() )
    {
        return true;
    }
    if ( behaviours & Option::IsBase64 )
    {
        xmlText = stringFromBase64 ( xmlText );
    }
    if ( xmlText == defvalue )
    {
        return true;
    }

    QString msg;
    return validatorX ( *this, { xmlText }, msg, errmsg );
}

bool FlagOption::allValid ( FlagOption& ref, QList<QString> args, QString& successmsg, QString& errmsg )
{
    if ( args.size() == 0 )
    {
        errmsg = "Missing arg";
        return false;
    }
    QString arg   = args.takeFirst();
    bool    ok    = true;
    bool    value = arg.toUShort ( &ok ) != 0;
    if ( !ok )
    {
        errmsg = QString ( "%1 must be zero or one" ).arg ( arg );
        return false;
    }
    ref.value  = value;
    successmsg = ref.validText.contains ( "%1" ) ? ref.validText.arg ( ref.value ) : ref.validText;
    return true;
}

FlagOption& FlagOption::operator= ( FlagOption rhs )
{
    TOption<bool>::operator= ( rhs );
    validator = rhs.validator;
    return *this;
}

bool FlagOption::readXmlDefault ( const QDomElement& xmlSection, QString& errmsg, FlagOption::FlagOptionValidator validatorX )
{
    QString xmlText;
    if ( !TOption<bool>::readXmlDefault ( *this, xmlSection, xmlText, errmsg ) )
    {
        return false;
    }
    if ( xmlText == QString() || xmlText.isEmpty() )
    {
        return true;
    }

    QString msg;
    return validatorX ( *this, { xmlText }, msg, errmsg );
}

bool IntOption::allValid ( IntOption& ref, QList<QString> args, QString& successmsg, QString& errmsg )
{
    if ( args.size() == 0 )
    {
        errmsg = "Missing arg";
        return false;
    }
    QString   arg   = args.takeFirst();
    bool      ok    = true;
    qlonglong value = arg.toLongLong ( &ok );
    if ( !ok )
    {
        errmsg = QString ( "%1 must be a number" ).arg ( arg );
    }
    if ( value < ref.minvalue || value > ref.maxvalue )
    {
        errmsg = QString ( "%1 must be no less than %2 and no more than %3" ).arg ( arg ).arg ( ref.minvalue ).arg ( ref.maxvalue );
        return false;
    }
    ref.value  = value;
    successmsg = ref.validText.contains ( "%1" ) ? ref.validText.arg ( ref.value ) : ref.validText;
    return true;
}

IntOption& IntOption::operator= ( IntOption rhs )
{
    TOption<qint64>::operator= ( rhs );
    minvalue  = rhs.minvalue;
    maxvalue  = rhs.maxvalue;
    validator = rhs.validator;
    return *this;
}

bool IntOption::readXmlDefault ( const QDomElement& xmlSection, QString& errmsg, IntOption::IntOptionValidator validatorXml )
{
    QString xmlText;
    if ( !TOption<qint64>::readXmlDefault ( *this, xmlSection, xmlText, errmsg ) )
    {
        return false;
    }
    if ( xmlText == QString() || xmlText.isEmpty() )
    {
        return true;
    }

    QString msg;
    return validatorXml ( *this, { xmlText }, msg, errmsg );
}

bool ByteArrayOption::allValid ( ByteArrayOption& ref, QList<QString> args, QString& successmsg, QString& errmsg )
{
    if ( args.size() == 0 )
    {
        errmsg = "Missing arg";
        return false;
    }
    ref.value  = byteArrayFromBase64 ( args.takeFirst() );
    successmsg = ref.validText;
    return true;
}

ByteArrayOption& ByteArrayOption::operator= ( ByteArrayOption rhs )
{
    TOption<QByteArray>::operator= ( rhs );
    validator = rhs.validator;
    return *this;
}

bool ByteArrayOption::readXmlDefault ( const QDomElement& xmlSection, QString& errmsg, ByteArrayOption::ByteArrayOptionValidator validatorX )
{
    QString xmlText;
    if ( !TOption<QByteArray>::readXmlDefault ( *this, xmlSection, xmlText, errmsg ) )
    {
        return false;
    }
    if ( xmlText == QString() || xmlText.isEmpty() )
    {
        return true;
    }
    // xmlText = stringFromBase64 ( xmlText ); -- because validator will call byteArrayFromBase64

    QString msg;
    return validatorX ( *this, { xmlText }, msg, errmsg );
}

bool StringVecOption::allValid ( VectorOption<QString>& ref, QList<QString> args, QString& successmsg, QString& errmsg )
{
    if ( args.size() == 0 )
    {
        errmsg = "Missing arg";
        return false;
    }

    QStringList argsX = args.takeFirst().split ( "\n" );
    if ( static_cast<size_t> ( argsX.size() ) > ref.maxsize )
    {
        errmsg = "Too many args";
        return false;
    }

    int j = 0;
    for ( size_t i = 0; i < ref.maxsize; i++ )
    {
        QString value = j < argsX.size() ? argsX.at ( j ) : "";
        if ( value.size() > 0 )
        {
            if ( ( ref.behaviours & Option::IsBase64 ) != 0 )
            {
                value = stringFromBase64 ( value );
            }
            ref.value.at ( static_cast<size_t> ( j ) ) = value;
            j++;
        }
    }

    successmsg = ref.validText.contains ( "%1" ) ? ref.validText.arg ( ref.toString() ) : ref.validText;

    return true;
}

QString StringVecOption::toString()
{
    QStringList sl;
    for ( ulong i = 0; i < value.size(); i++ )
    {
        sl += value.at ( i );
    }
    return sl.join ( ", " );
}

QString StringVecOption::toXmlText()
{
    QStringList sl;
    for ( ulong i = 0; i < value.size(); i++ )
    {
        sl += value.at ( i );
    }
    return sl.join ( "\n" ).toUtf8().toBase64();
}

bool FlagVecOption::allValid ( VectorOption<bool>& ref, QList<QString> args, QString& successmsg, QString& errmsg )
{
    if ( args.size() == 0 )
    {
        errmsg = "Missing arg";
        return false;
    }

    QStringList argsX = args.takeFirst().split ( "\n" );
    if ( static_cast<size_t> ( argsX.size() ) > ref.maxsize )
    {
        errmsg = "Too many args";
        return false;
    }

    int j = 0;
    for ( size_t i = 0; i < ref.maxsize; i++ )
    {
        QString arg = j < argsX.size() ? argsX.at ( j ) : "";
        if ( arg.size() > 0 )
        {
            bool ok    = true;
            bool value = arg.toUShort ( &ok ) != 0;
            if ( !ok )
            {
                errmsg = QString ( "%1 must be zero or one" ).arg ( arg );
                return false;
            }
            ref.value.Add ( value );
            j++;
        }
    }

    successmsg = ref.validText.contains ( "%1" ) ? ref.validText.arg ( ref.toString() ) : ref.validText;
    return true;
}

QString FlagVecOption::toString()
{
    QStringList sl;
    for ( ulong i = 0; i < value.size(); i++ )
    {
        sl += QString ( "%1" ).arg ( value.at ( i ) ? "t" : "f" );
    }
    return sl.join ( ", " );
}

QString FlagVecOption::toXmlText()
{
    QStringList sl;
    for ( ulong i = 0; i < value.size(); i++ )
    {
        sl += QString ( "%1" ).arg ( value.at ( i ) );
    }
    return sl.join ( "\n" ).toUtf8().toBase64();
}

bool IntVecOption::allValid ( VectorOption<int>& ref, QList<QString> args, QString& successmsg, QString& errmsg )
{
    if ( args.size() == 0 )
    {
        errmsg = "Missing arg";
        return false;
    }

    QStringList argsX = args.takeFirst().split ( "\n" );
    if ( static_cast<size_t> ( argsX.size() ) > ref.maxsize )
    {
        errmsg = "Too many args";
        return false;
    }

    int j = 0;
    for ( size_t i = 0; i < ref.maxsize; i++ )
    {
        QString arg = j < argsX.size() ? argsX.at ( j ) : "";
        if ( arg.size() > 0 )
        {
            bool ok    = true;
            int  value = arg.toInt ( &ok );
            if ( !ok )
            {
                errmsg = QString ( "%1 must be a valid number" ).arg ( arg );
                return false;
            }
            ref.value.Add ( value );
            j++;
        }
    }

    successmsg = ref.validText.contains ( "%1" ) ? ref.validText.arg ( ref.toString() ) : ref.validText;
    return true;
}

QString IntVecOption::toString()
{
    QStringList sl;
    for ( ulong i = 0; i < value.size(); i++ )
    {
        sl += QString ( "%1" ).arg ( value.at ( i ) );
    }
    return sl.join ( ", " );
}

QString IntVecOption::toXmlText()
{
    QStringList sl;
    for ( ulong i = 0; i < value.size(); i++ )
    {
        sl += QString ( "%1" ).arg ( value.at ( i ) );
    }
    return sl.join ( "\n" ).toUtf8().toBase64();
}

JamulusOptions::JamulusOptions() :

    // Exits
    // Show help and exit --------------------------------------------------
    fo_help ( "help", Option::Exits, { "-h", "-?" }, { "--help" }, "display this help text and exit" ),

    // Produce HTML help and exit ------------------------------------------
    // Undocumented documentation command line argument
    fo_helphtml ( "helphtml", Option::Exits, {}, { "--helphtml" }, QString() /* display this HTML help text and exit */ ),

    // Show version, etc and exit ------------------------------------------
    fo_version ( "version", Option::Exits, { "-v" }, { "--version" }, "display version, copyright, licence and warranty information and exit" ),

    // Early Common
    // Settings file name --------------------------------------------------
    so_inifile (
        "inifile",
        Option::Early | Option::Common,
        { "-i" },
        { "--inifile" },
        "initialization file name (not supported when --server and --nogui are used together)",
        "- initialization file name: %1",
        "Jamulus.ini",
        [] ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg ) {
            return StringOption::validFile ( ref, args, successmsg, errmsg, "Initialization file name is not valid - %1.", false, false, false );
        } ),

    // Headless flag -------------------------------------------------------
    fo_nogui ( "nogui", Option::Early | Option::Common, { "-n" }, { "--nogui" }, "disable GUI (\"headless\")", "- no GUI mode chosen" ),

    // Quiet start up flag -------------------------------------------------
    fo_quiet ( "quiet",
               Option::Early | Option::Common,
               {},
               { "--quiet" },
               "suppress start up output - error and runtime messages are still displayed" ),

    // Common
    // IPv6 enabled flag ---------------------------------------------------
    fo_ipv6 ( "ipv6",
              Option::Common | Option::Setting,
              { "-6" },
              { "--enableipv6" },
              "enable IPv6 addressing (IPv4 is always enabled)",
              "- IPv6 enabled" ),

    // JSON-RPC port number ------------------------------------------------
    io_jsonrpcport ( "jsonrpcport",
                     Option::Common | Option::Setting,
                     {},
                     { "--jsonrpcport" },
                     "enable JSON-RPC server, set TCP port number (EXPERIMENTAL, APIs might still change; only accessible from localhost)",
                     "- selected JSON-RPC port number: %1",
                     INVALID_PORT,
                     0,
                     65535 ),

    // JSON RPC secret file name -------------------------------------------
    so_jsonrpcsecret (
        "jsonrpcsecret",
        Option::Common | Option::Setting | Option::IsBase64,
        {},
        { "--jsonrpcsecretfile" },
        "path to a single-line file which contains a freely-chosen secret (subject to minimum length) to authenticate JSON-RPC users.",
        "- JSON-RPC secret file name: %1",
        QString(),
        [] ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg ) {
            QString arg = args.at ( 0 );
            if ( !StringOption::validFile ( ref, args, successmsg, errmsg, "The JSON-RPC secret file name is not valid - %1", true, true, true ) )
            {
                return false;
            }
            ref.value = ref.value.trimmed();
            if ( ref.value.length() < JSON_RPC_MINIMUM_SECRET_LENGTH )
            {
                errmsg = QString ( "The JSON-RPC secret is only %1.  Please use a minimum length of %2." )
                             .arg ( ref.value.length() )
                             .arg ( JSON_RPC_MINIMUM_SECRET_LENGTH );
                ref.value = QString();
                return false;
            }
            successmsg = ref.validText.arg ( arg );
            return true;
        },
        [] ( StringOption& ref, const QDomElement& xmlSection, QString& errmsg ) {
            return ref.readXmlDefault ( xmlSection, errmsg, [] ( StringOption& refX, QList<QString> argsX, QString&, QString& errmsgX ) {
                refX.value = argsX.takeFirst().trimmed();
                if ( refX.value.length() < JSON_RPC_MINIMUM_SECRET_LENGTH )
                {
                    errmsgX = QString ( "The JSON-RPC secret is only %1.  Please use a minimum length of %2." )
                                  .arg ( refX.value.length() )
                                  .arg ( JSON_RPC_MINIMUM_SECRET_LENGTH );
                    refX.value = QString();
                    return false;
                }
                return true;
            } );
        } ),

    // Server Bind IP --------------------------------------------------
    so_jsonrpcbindip (
        "jsonrpcbindip",
        Option::Common | Option::Setting,
        {},
        { "--jsonrpcbindip" },
        "network interface IP address the JSON-RPC Server will bind to (defaults to localhost)",
        "- JSON-RPC Server will bind to interface with IP: %1 (if enabled)",
        DEFAULT_JSON_RPC_LISTEN_ADDRESS,
        [] ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg ) {
            successmsg = QString();
            ref.value  = args.takeFirst().trimmed();
            if ( ref.value.isEmpty() )
            {
                errmsg = "Missing network interface IP address value";
                return false;
            }
            QHostAddress InetAddr;
            if ( !InetAddr.setAddress ( ref.value ) )
            {
                errmsg = QString ( "%1 is not a valid network interface IP address value. Only plain IP addresses are supported." ).arg ( ref.value );
                ref.value = QString();
                return false;
            }
            successmsg += ref.validText.arg ( ref.value );
            return true;
        } ),

    // UI Language ---------------------------------------------------------
    so_language ( "language",
                  Option::Common | Option::Setting,
                  {},
                  { "--language" },
                  "two letter code for language to use for display",
                  "- language chosen is '%1'",
                  CLocale::FindSysLangTransFileName ( CLocale::GetAvailableTranslations() ).first,
                  [] ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg ) {
                      ref.value = args.takeFirst();

                      const QMap<QString, QString>& TranslMap = CLocale::GetAvailableTranslations();
                      if ( TranslMap.constFind ( ref.value ) != TranslMap.constEnd() )
                      {
                          successmsg = ref.validText.arg ( ref.value );
                      }
                      else
                      {
                          errmsg    = QString ( "'%1' is not a supported language code" ).arg ( ref.value );
                          ref.value = QString();
                          return false;
                      }

                      return true;
                  } ),

    // no translation flag -------------------------------------------------
    fo_notranslation ( "notranslation",
                       Option::Common,
                       { "-t" },
                       { "--notranslation" },
                       "disable translation (use English language)",
                       "- running in English" ),

    // Port number ---------------------------------------------------------
    io_port ( "port",
              Option::Common,
              { "-p" },
              { "--port" },
              "set the local port number",
              "- selected port number: %1",
              DEFAULT_PORT_NUMBER,
              0,
              65535 ),

    // Quality of Service number -------------------------------------------
    io_qos ( "qos",
             Option::Common | Option::Setting,
             { "-Q" },
             { "--qos" },
             QString ( "set the QoS value (default is 0x%1; 0 to disable; see the Jamulus website to enable QoS on Windows)" ).arg ( 0x80, 0, 16 ),
             "- selected port number: %1",
             0x80,
             0,
             255 ),

    // Server only
    // Disconnect all clients on quit --------------------------------------
    fo_discononquit ( "discononquit",
                      Option::Server | Option::Setting,
                      { "-d" },
                      { "--discononquit" },
                      "disconnect all Clients on quit",
                      "- disconnect all Clients on quit" ),

    // Directory address ---------------------------------------------------
    so_directory (
        "directoryaddress",
        Option::Server | Option::Setting,
        { "-e" },
        { "--directoryaddress" },
        "network address of the Directory with which to register (or 'localhost' to run as a Directory)",
        "- register with Directory at address: %1",
        QString(),
        [] ( StringOption& ref, QList<QString> args, QString& successmsg, QString& ) {
            ref.value = args.takeFirst();
            if ( ref.value.compare ( "localhost", Qt::CaseInsensitive ) == 0 || ref.value.compare ( "127.0.0.1" ) == 0 )
            {
                successmsg = "- running as Directory";
            }
            else
            {
                successmsg = ref.validText.arg ( ref.value );
            }
            return true;
        },
        [] ( StringOption& ref, const QDomElement& xmlSection, QString& errmsg ) {
            bool ret = ref.readXmlDefault ( xmlSection, errmsg, ref.validator );
            if ( !ret )
            {
                return false;
            }
            if ( ref.value != ref.defvalue )
            {
                return true;
            }
            StringOption so = StringOption ( ref );
            so.name         = "centralservaddr";
            so.xmlReader    = [] ( StringOption& refX, const QDomElement& xmlSectionX, QString& errmsgX ) {
                return refX.readXmlDefault ( xmlSectionX, errmsgX, refX.validator );
            };
            if ( so.readXML ( xmlSection, errmsg ) && so.value != so.defvalue )
            {
                ref.value = so.value;
            }
            return true;
        } ),

    // Deprecated ----------------------------------------------------------
    so_directoryDeprecated ( "directoryDeprecated",
                             Option::Server,
                             {},
                             { "--directoryserver", "--centralserver" },
                             QString(),
                             so_directory.validText,
                             QString(),
                             so_directory.validator ),

    // Directory type (client connect dialog / server main tab) ------------
    // +++ needs "backwards compatibility" when reading client settings ++??what about server??
    // < 3.4.7 -> defcentservaddr set directorytype AT_DEFAULT even if centralservaddr set
    // < 3.8.2 -> centservaddrtype
    io_directorytype ( "directorytype",
                       Option::Common | Option::Setting,
                       AT_DEFAULT,
                       AT_DEFAULT,
                       AT_CUSTOM,
                       [] ( IntOption& ref, const QDomElement& xmlSection, QString& errmsg ) {
                           bool ret = ref.readXmlDefault ( xmlSection, errmsg, ref.validator );
                           if ( !ret )
                           {
                               return false;
                           }
                           if ( AllOptions.fo_server.value || ref.value != ref.defvalue )
                           {
                               return true;
                           }
                           IntOption io  = IntOption ( ref );
                           io.name       = "centservaddrtype";
                           io.behaviours = Option::Client /* ** NOTE ** */ | Option::Setting;
                           io.value = io.defvalue = AT_DEFAULT; // to be explicit
                           if ( io.readXmlDefault ( xmlSection, errmsg, io.validator ) && io.value != io.defvalue )
                           {
                               ref.value = io.value;
                           }
                           else
                           {
                               FlagOption fo =
                                   FlagOption ( "defcentservaddr", Option::Client /* ** NOTE ** */ | Option::Setting, FlagOption::allValid );
                               fo.value = fo.defvalue = false; // to be explicit
                               if ( fo.readXmlDefault ( xmlSection, errmsg, fo.validator ) && fo.value != fo.defvalue )
                               {
                                   ref.value = AT_DEFAULT;
                               }
                           }

                           return true;
                       } ),

    // Server list persistence file ----------------------------------------
    so_serverlistfile (
        "serverlistfile",
        Option::Server | Option::Setting | Option::IsBase64,
        {},
        { "--directoryfile", "--serverlistfile" },
        "file to hold server list across Directory restarts. Directories only.",
        "- server list persistence file: %1",
        QString(),
        [] ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg ) {
            return StringOption::validFile ( ref,
                                             args,
                                             successmsg,
                                             errmsg,
                                             "The server list persistence file name is not valid - %1.",
                                             false,
                                             false,
                                             false );
        },
        [] ( StringOption& ref, const QDomElement& xmlSection, QString& errmsg ) {
            bool ret = ref.readXmlDefault ( xmlSection, errmsg, ref.validator );
            if ( !ret )
            {
                return false;
            }
            if ( ref.value != ref.defvalue )
            {
                return true;
            }
            StringOption so = StringOption ( ref );
            so.name         = "directoryfile";
            so.xmlReader    = [] ( StringOption& refX, const QDomElement& xmlSectionX, QString& errmsgX ) {
                return refX.readXmlDefault ( xmlSectionX, errmsgX, refX.validator );
            };
            if ( so.readXML ( xmlSection, errmsg ) && so.value != so.defvalue )
            {
                ref.value = so.value;
            }
            return true;
        } ),

    // Server list filter --------------------------------------------------
    vs_serverlistfilter (
        "serverlistfilter",
        Option::Server | Option::Setting,
        { "-f" },
        { "--listfilter" },
        "server list accepted addresses; Directories only.  Format: [IP address 1];[IP address 2];[IP address 3]; ...",
        "- server list filter set",
        1,
        0,
        std::numeric_limits<size_t>::max(),
        CVector<QString>(),
        CVector<QString>(),
        [] ( VectorOption<QString>& ref, QList<QString> args, QString& successmsg, QString& errmsg ) {
            bool seenEmpty = false;
            successmsg     = QString();
            QString arg    = args.takeFirst().trimmed();
            if ( arg.isEmpty() )
            {
                errmsg = "Missing server list filter value";
                return false;
            }
            QStringList slServerListFilter = arg.split ( ";" );
            for ( int iIdx = 0; iIdx < slServerListFilter.size(); iIdx++ )
            {
                arg = slServerListFilter.at ( iIdx ).trimmed();
                // check for special case: [version]
                if ( ( arg.length() > 2 ) && ( arg.at ( 1 ) == '[' ) && ( arg.at ( arg.length() - 1 ) == ']' ) )
                {
                    // Good case - it seems QVersionNumber isn't fussy
                    ref.value.Add ( arg );
                }
                else if ( arg.isEmpty() && !seenEmpty )
                {
                    seenEmpty = true;
                    successmsg += "There was an empty entry in the server list filter that has been ignored.\n";
                }
                else
                {
                    QHostAddress InetAddr;
                    if ( !InetAddr.setAddress ( arg ) )
                    {
                        errmsg = QString ( "%1 is not a valid server list filter entry. Only plain IP addresses are supported." ).arg ( arg );
                        return false;
                    }
                    ref.value.Add ( arg );
                }
            }
            successmsg += ref.validText;
            return true;
        },
        [] ( VectorOption<QString>& ref, const QDomElement& xmlSection, QString& errmsg ) {
            return ref.readXmlDefault ( xmlSection,
                                        errmsg,
                                        [] ( VectorOption<QString>& refX, QList<QString> argsX, QString& successmsgX, QString& errmsgX ) {
                                            QString arg = Option::stringFromBase64 ( argsX.takeFirst().trimmed() ).split ( "\n" ).join ( ";" );
                                            return arg.isEmpty() ? true : refX.validator ( refX, { arg }, successmsgX, errmsgX );
                                        } );
        } ),

    // Use 64 samples frame size mode --------------------------------------
    fo_fastupdate ( "fastupdate",
                    Option::Server | Option::Setting,
                    { "-F" },
                    { "--fastupdate" },
                    "use 64 samples frame size mode",
                    QString ( "- using %1 samples frame size mode" ).arg ( SYSTEM_FRAME_SIZE_SAMPLES ) ),

    // Use logging ---------------------------------------------------------
    so_logfile (
        "logfile",
        Option::Server | Option::Setting,
        { "-l" },
        { "--log" },
        "enable logging, set file name",
        "- logging file name: %1",
        QString(),
        [] ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg ) {
            return StringOption::validFile ( ref, args, successmsg, errmsg, "The server log file name is not valid - %1.", false, false, false );
        } ),

    // Use licence flag ----------------------------------------------------
    fo_licence ( "licence",
                 Option::Server | Option::Setting,
                 { "-L" },
                 { "--licence" },
                 "show an agreement window before users can connect",
                 "- licence required" ),

    // HTML status file ----------------------------------------------------
    so_htmlstatus ( "htmlstatus",
                    Option::Server | Option::Setting,
                    { "-m" },
                    { "--htmlstatus" },
                    "enable HTML status file, set file name",
                    "- HTML status file name: %1",
                    QString(),
                    [] ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg ) {
                        return StringOption::validFile ( ref,
                                                         args,
                                                         successmsg,
                                                         errmsg,
                                                         "The server HTML status file name is not valid - %1.",
                                                         false,
                                                         false,
                                                         false );
                    } ),

    // Server info ---------------------------------------------------------
    so_serverinfo ( "serverinfo",
                    Option::Server | Option::Setting | Option::IsBase64,
                    { "-o" },
                    { "--serverinfo" },
                    "registration info for this Server.  Format: [name];[city];[country as two-letter ISO country code or Qt5 QLocale ID]\n",
                    "- Server registration info: %1",
                    QString(),
                    [] ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg ) {
                        successmsg = QString();
                        ref.value  = args.takeFirst().trimmed();
                        if ( ref.value.isEmpty() )
                        {
                            errmsg = "Missing registration info for this Server.";
                            return false;
                        }

                        QStringList slServInfoSeparateParams = ref.value.split ( ";" );
                        if ( slServInfoSeparateParams.count() < 3 )
                        {
                            errmsg = "\"--serverinfo '%1'\": must contain [name];[city];[country]";
                            return false;
                        }

                        bool      ok;
                        const int iCountry = slServInfoSeparateParams[2].toInt ( &ok );
                        if ( ok )
                        {
                            if ( iCountry == 0 || !CLocale::IsCountryCodeSupported ( iCountry ) )
                            {
                                errmsg = "'country' code is not a valid QLocale country ID.";
                                return false;
                            }
                        }
                        else
                        {
                            QLocale::Country qlCountry = CLocale::GetCountryCodeByTwoLetterCode ( slServInfoSeparateParams[2] );
                            if ( qlCountry == QLocale::AnyCountry )
                            {
                                errmsg = "'country' code is not a valid two-letter ISO country identifier.";
                                return false;
                            }
                        }

                        successmsg += ref.validText.arg ( ref.value );
                        return true;
                    },
                    [] ( StringOption& ref, const QDomElement& xmlSection, QString& errmsg ) {
                        bool ret = ref.readXmlDefault ( xmlSection, errmsg, ref.validator );
                        if ( !ret )
                        {
                            return false;
                        }
                        if ( ref.value != ref.defvalue )
                        {
                            return true;
                        }
                        QString      name, city, country;
                        StringOption so = StringOption ( ref );

                        so.name      = "name";
                        so.xmlReader = [] ( StringOption& refX, const QDomElement& xmlSectionX, QString& errmsgX ) {
                            return refX.readXmlDefault ( xmlSectionX, errmsgX, StringOption::allValid );
                        };
                        if ( so.readXML ( xmlSection, errmsg ) && so.value != so.defvalue )
                        {
                            name = so.value;
                        }

                        so.name = "city";
                        if ( so.readXML ( xmlSection, errmsg ) && so.value != so.defvalue )
                        {
                            city = so.value;
                        }

                        IntOption io = IntOption ( "country", ref.behaviours, -1, 0, static_cast<int> ( QLocale::LastCountry ), IntOption::allValid );
                        if ( io.readXML ( xmlSection, errmsg ) && io.value != io.defvalue )
                        {
                            country = QString::number ( io.value );
                        }

                        ref.value = QString ( "%1;%2;%3" ).arg ( name, city, country );
                        return true;
                    } ),

    // Server Public IP ----------------------------------------------------
    so_serverpublicip ( "serverpublicip",
                        Option::Server | Option::Setting,
                        {},
                        { "--serverpublicip" },
                        "public IP address to be supplied by the Directory (needed when registering a Server on the same LAN)",
                        "- Directory pulic ip: %1",
                        QString(),
                        [] ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg ) {
                            successmsg = QString();
                            ref.value  = args.takeFirst().trimmed();
                            if ( ref.value.isEmpty() )
                            {
                                errmsg = "Missing Directory public ip value";
                                return false;
                            }
                            QHostAddress InetAddr;
                            if ( !InetAddr.setAddress ( ref.value ) )
                            {
                                errmsg    = QString ( "%1 is not a valid IP value. Only plain IP addresses are supported." ).arg ( ref.value );
                                ref.value = QString();
                                return false;
                            }
                            successmsg += ref.validText.arg ( ref.value );
                            return true;
                        } ),

    // Enable delay panning on startup -------------------------------------
    fo_delaypan ( "delaypan",
                  Option::Server | Option::Setting,
                  { "-P" },
                  { "--delaypan" },
                  "start with delay panning enabled",
                  "- delay panning enabled" ),

    // Recording directory -------------------------------------------------
    so_recording ( "recordingdir",
                   Option::Server | Option::Setting | Option::IsBase64,
                   { "-R" },
                   { "--recording" },
                   "set server recording directory; server will record when a session is active by default",
                   "- recording directory name: %1 (active)",
                   QString(),
                   [] ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg ) {
                       return StringOption::validDir ( ref, args, successmsg, errmsg, "The server recording path is not valid - %1.", false );
                   } ),

    // Disable recording on startup ----------------------------------------
    fo_norecord ( "norecord",
                  Option::Server | Option::Setting,
                  {},
                  { "--norecord" },
                  "set server not to record by default when recording is configured",
                  "- recording will not take place until enabled" ),

    // Server mode flag ----------------------------------------------------
    fo_server ( "server", Option::Early | Option::Server, { "-s" }, { "--server" }, "run as Server (rather than Client)", "- Server mode chosen" ),

    // Server Bind IP --------------------------------------------------
    so_serverbindip (
        "serverbindip",
        Option::Server | Option::Setting,
        {},
        { "--serverbindip" },
        "network interface IP address the Server will bind to (rather than all)",
        "- Server will bind to interface with IP: %1",
        QString(),
        [] ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg ) {
            successmsg = QString();
            ref.value  = args.takeFirst().trimmed();
            if ( ref.value.isEmpty() )
            {
                errmsg = "Missing network interface IP address value";
                return false;
            }
            QHostAddress InetAddr;
            if ( !InetAddr.setAddress ( ref.value ) )
            {
                errmsg = QString ( "%1 is not a valid network interface IP address value. Only plain IP addresses are supported." ).arg ( ref.value );
                ref.value = QString();
                return false;
            }
            successmsg += ref.validText.arg ( ref.value );
            return true;
        } ),

    // Use multithreading --------------------------------------------------
    fo_multithreading ( "multithreading",
                        Option::Server | Option::Setting,
                        { "-T" },
                        { "--multithreading" },
                        "use multithreading to make better use of multi-core CPUs and support more Clients",
                        "- using multithreading" ),

    // Maximum number of channels ------------------------------------------
    io_numchannels ( "numchannels",
                     Option::Server | Option::Setting,
                     { "-u" },
                     { "--numchannels" },
                     QString ( "set the maximum number of channels (default %1, max %2)" ).arg ( 10 ).arg ( MAX_NUM_CHANNELS ),
                     "- maximum number of channels: %1",
                     10,
                     1,
                     MAX_NUM_CHANNELS ),

    // Server welcome message ----------------------------------------------
    so_welcomemessage ( "welcome",
                        Option::Server | Option::Setting,
                        { "-w" },
                        { "--welcomemessage" },
                        "welcome message to display on connect (string or filename)",
                        "- welcome message set",
                        QString(),
                        [] ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg ) {
                            ref.value = args.takeFirst().trimmed();
                            if ( ref.value.isEmpty() )
                            {
                                successmsg = "** empty welcome message ignored";
                                ref.value  = QString();
                                return true;
                            }
                            QFileInfo welcomeMsgFileInfo ( ref.value );
                            if ( welcomeMsgFileInfo.exists() )
                            {
                                if ( !welcomeMsgFileInfo.isReadable() )
                                {
                                    errmsg =
                                        QString ( "To use a file, it needs to be readable.  Please check the permissions on %1." ).arg ( ref.value );
                                    ref.value = QString();
                                    return false;
                                }
                            }
                            successmsg = ref.validText;
                            return true;
                        } ),

    // Start minimized -----------------------------------------------------
    fo_startminimized ( "autostartmin",
                        Option::Server | Option::Setting,
                        { "-z" },
                        { "--startminimized" },
                        "start minimized (not available when running headless)",
                        "- starting minimized" ),

    // Client only:
    // Connect on startup --------------------------------------------------
    so_connect ( "connect",
                 Option::Client,
                 { "-c" },
                 { "--connect" },
                 "connect to given Server on startup",
                 "- connect on startup to Server: %1",
                 QString() ),

    // Disabling auto Jack connections -------------------------------------
    fo_nojackconnect ( "nojackconnect",
                       Option::Client | Option::Setting,
                       { "-j" },
                       { "--nojackconnect" },
                       "disable auto JACK connections",
                       "- no automatic JACK connection" ),

    // Mute stream on startup ----------------------------------------------
    // it would be nice if the help text was helpful...
    fo_mutestream ( "mutestream",
                    Option::Client,
                    { "-M" },
                    { "--mutestream" },
                    "prevent others on a server from hearing what I play",
                    "- others on a server will not hear you (mutestream active)" ),

    // For headless client mute my own signal in personal mix --------------
    // it would be nice if the help text was helpful... and why headless-only?
    fo_mutemyown ( "mutemyown",
                   Option::Client,
                   {},
                   { "--mutemyown" },
                   "prevent me from hearing what I play in the server mix",
                   "- you will not hear yourself in the server mix (mutemyown active)" ),

    // Client Name ---------------------------------------------------------
    so_clientname ( "clientname",
                    Option::Client | Option::Setting | Option::IsBase64,
                    {},
                    { "--clientname" },
                    "Client name (window title and JACK client name)",
                    "- Client name: %1",
                    QString() ),

    // MIDI controls -------------------------------------------------------
    so_ctrlmidich ( "ctrlmidich",
                    Option::Client,
                    {},
                    { "--ctrlmidich" },
                    "MIDI controller settings (see wiki for format)",
                    "- MIDI controller settings: %1",
                    QString() ),

    // Audio Alerts --------------------------------------------------------
    fo_audioalerts ( "enableaudioalerts",
                     Option::Client,
                     {},
                     { "--audioalerts" },
                     "sound an audio alter when someone joins a server or a chat message is received",
                     "- audio alerts active" ),

    // Additional undocumented client options
    // Show full server list -----------------------------------------------
    // Undocumented debugging command line argument:
    // Show full server list regardless of whether a ping response is received.
    fo_showallservers ( "showallservers", Option::Client, {}, { "--showallservers" }, QString() ),

    // Show analyzer console -----------------------------------------------
    // Undocumented debugging command line argument:
    // Show the analyzer console to debug network buffer properties.
    fo_showanalyzerconsole ( "showanalyzerconsole", Option::Client, {}, { "--showanalyzerconsole" }, QString() ),

    /**********\
    * Settings *
    \**********/

    // Manually entered server addresses (connect dialog) ------------------
    // +++ needs "backwards compatibility" when reading settings -> historically stored as ipaddress%i
    vo_serveraddresses ( "serveraddresses",
                         Option::Client | Option::Setting,
                         MAX_NUM_SERVER_ADDR_ITEMS,
                         StringVecOption::allValid,
                         [] ( VectorOption<QString>& ref, const QDomElement& xmlSection, QString& errmsg ) {
                             bool ret = ref.readXmlDefault (
                                 xmlSection,
                                 errmsg,
                                 [] ( VectorOption<QString>& refX, QList<QString> argsX, QString& successmsgX, QString& errmsgX ) {
                                     return refX.validator ( refX, { Option::stringFromBase64 ( argsX.takeFirst() ) }, successmsgX, errmsgX );
                                 } );
                             if ( !ret )
                             {
                                 return false;
                             }
                             if ( ref.isEmpty() )
                             {
                                 // maybe the old format was used
                                 QList<QString> args;
                                 for ( int i = 0; i < MAX_NUM_SERVER_ADDR_ITEMS; i++ )
                                 {
                                     StringOption so = StringOption ( QString ( "ipaddress%1" ).arg ( i ), ref.behaviours );
                                     if ( so.readXML ( xmlSection, errmsg ) && so.value.size() > 0 )
                                     {
                                         args.append ( so.value );
                                     }
                                 }
                                 if ( args.size() > 0 )
                                 {
                                     QString msg;
                                     if ( !ref.validator ( ref, { args.join ( "\n" ) }, msg, errmsg ) )
                                     {
                                         return false;
                                     }
                                 }
                             }
                             return true;
                         } ),

    // New client level (settings->advanced) -------------------------------
    io_newclientlevel ( "newclientlevel", Option::Client | Option::Setting, 100, 0, 100 ),

    // Input boost (settings->advanced) ------------------------------------
    io_inputboost ( "inputboost", Option::Client | Option::Setting, 1, 1, 10 ),

    // Enable feedback detection (settings->advanced) ----------------------
    fo_detectfeedback ( "enablefeedbackdetection", Option::Client | Option::Setting, FlagOption::allValid ),

    // Show all musicians (connect dialog) ---------------------------------
    fo_connectdlgshowallmusicians ( "connectdlgshowallmusicians", Option::Client | Option::Setting, FlagOption::allValid ),

    // Channel sort (mixer) ------------------------------------------------
    io_channelsort ( "channelsort", Option::Client | Option::Setting, ST_NO_SORT, ST_NO_SORT, ST_BY_CITY ),

    // Own fader first (mixer) ---------------------------------------------
    fo_ownfaderfirst ( "ownfaderfirst", Option::Client | Option::Setting, FlagOption::allValid ),

    // Number of mixer panel rows (mixer) ----------------------------------
    io_numrowsmixpan ( "numrowsmixpan", Option::Client | Option::Setting, 1, 1, 8 ),

    // Name (profile) ------------------------------------------------------
    // set from storage with fromBase64String(...)
    // use toBase64String() to get the string for storage
    so_name ( "name", Option::Client | Option::Setting | Option::IsBase64, "No Name", StringOption::allValid ),

    // Instrument (profile) ------------------------------------------------
    io_instrument ( "instrument", Option::Client | Option::Setting, 0, 0, CInstPictures::GetNumAvailableInst() - 1 ),

    // Country (profile) ---------------------------------------------------
    io_country ( "country",
                 Option::Client | Option::Setting,
                 static_cast<int> ( QLocale::system().country() ),
                 0,
                 static_cast<int> ( QLocale::LastCountry ) ),

    // City (profile) ------------------------------------------------------
    // set from storage with fromBase64String(...)
    // use toBase64String() to get the string for storage
    so_city ( "city", Option::Client | Option::Setting | Option::IsBase64, StringOption::allValid ),

    // Skill (profile) -----------------------------------------------------
    io_skill ( "skill", Option::Client | Option::Setting, SL_NOT_SET, SL_NOT_SET, SL_PROFESSIONAL ),

    // Input pan -----------------------------------------------------------
    io_audfad ( "audfad", Option::Client | Option::Setting, 50 /*AUD_FADER_IN_MIDDLE*/, 0 /*AUD_FADER_IN_MIN*/, 100 /*AUD_FADER_IN_MAX*/ ),

    // Audio reverb level --------------------------------------------------
    io_revlev ( "revlev", Option::Client | Option::Setting, 0, 0, 100 ),

    // Reverb on left (not right) ------------------------------------------
    fo_reverblchan ( "reverblchan", Option::Client | Option::Setting, FlagOption::allValid ),

    // Audio device (audio device) -----------------------------------------
    // set from storage with fromBase64String(...)
    // use toBase64String() to get the string for storage
    // (set from auddev first; then other values)
    // +++ remember the first call to pClient->SetSndCrdDev ( ... ) can return an error to be displayed/logged
    so_auddev ( "auddev", Option::Client | Option::Setting, StringOption::allValid ),

    // Left input channel (audio device) ----------------------------------
    io_sndcrdinlch ( "sndcrdinlch", Option::Client | Option::Setting, 0, 0, MAX_NUM_IN_OUT_CHANNELS - 1 ),

    // Right input channel (audio device) ---------------------------------
    io_sndcrdinrch ( "sndcrdinrch", Option::Client | Option::Setting, 1, 0, MAX_NUM_IN_OUT_CHANNELS - 1 ),

    // Left output channel (audio device) ---------------------------------
    io_sndcrdoutlch ( "sndcrdoutlch", Option::Client | Option::Setting, 0, 0, MAX_NUM_IN_OUT_CHANNELS - 1 ),

    // Right output channel (audio device) --------------------------------
    io_sndcrdoutrch ( "sndcrdoutrch", Option::Client | Option::Setting, 1, 0, MAX_NUM_IN_OUT_CHANNELS - 1 ),

    // Requested buffer size (audio device) -------------------------------
    io_prefsndcrdbufidx (
        "prefsndcrdbufidx",
        Option::Client | Option::Setting,
        [] ( IntOption& ref, const QDomElement& xmlSection, QString& errmsg ) {
            return ref.readXmlDefault ( xmlSection, errmsg, [] ( IntOption& refX, QList<QString> argsX, QString&, QString& errmsgX ) {
                QString     arg = argsX.takeFirst();
                QStringList sl{ "1", "2", "4" };
                refX.value = sl.indexOf ( arg );
                if ( refX.value < 0 )
                {
                    errmsgX = QString ( "You can only supply one these values: %1" ).arg ( sl.join ( ", " ) );
                    return false;
                }
                refX.value = QList<int>{ 1, 2, 4 }.at ( static_cast<int> ( refX.value ) );
                return true;
            } );
        } ),

    // Audio channels ------------------------------------------------------
    io_audiochannels ( "audiochannels", Option::Client | Option::Setting, CC_STEREO, CC_MONO, CC_STEREO ),

    // Audio quality -------------------------------------------------------
    io_audioquality ( "audioquality", Option::Client | Option::Setting, AQ_NORMAL, AQ_LOW, AQ_HIGH ),

    // Use 64 samples per frame through the OPUS CODEC ---------------------
    fo_enableopussmall ( "enableopussmall", Option::Client | Option::Setting, FlagOption::allValid ),

    // Automatic network jitter buffer size setting ------------------------
    fo_autojitbuf ( "autojitbuf", Option::Client | Option::Setting, FlagOption::allValid ),

    // Local network jitter buffer size ------------------------------------
    io_jitbuf ( "jitbuf", Option::Client | Option::Setting, MAX_NET_BUF_SIZE_NUM_BL / 2, MIN_NET_BUF_SIZE_NUM_BL, MAX_NET_BUF_SIZE_NUM_BL ),

    // Server network jitter buffer size -----------------------------------
    io_jitbufserver ( "jitbufserver", Option::Client | Option::Setting, DEF_NET_BUF_SIZE_NUM_BL, MIN_NET_BUF_SIZE_NUM_BL, MAX_NET_BUF_SIZE_NUM_BL ),

    // GUI design ----------------------------------------------------------
    io_guidesign ( "guidesign", Option::Client | Option::Setting, GD_ORIGINAL, GD_STANDARD, GD_SLIMFADER ),

    // MeterStyle ----------------------------------------------------------
    io_meterstyle ( "meterstyle", Option::Client | Option::Setting, MT_BAR_NARROW, MT_BAR_NARROW, MT_LED_ROUND_BIG ),

    // Custom directory index (connect dialog) -----------------------------
    // +++ needs handling with care: should be set to zero if directorytype not AT_CUSTOM
    // +++ also must not exceed size of vo_directoryaddresses
    io_customdirectoryindex ( "customdirectoryindex", Option::Client | Option::Setting, 0, 0, MAX_NUM_SERVER_ADDR_ITEMS ),

    // Manually entered "custom" directory addresses (settings->advanced) --
    // +++ needs "backwards compatibility" when reading settings
    // < 3.6.1 -> centralservaddr and set directorytype AT_CUSTOM unless defcentservaddr present
    // < 3.8.2 -> centralservaddr%1
    // and then directoryaddress%1
    vo_directoryaddresses ( "directoryaddresses",
                            Option::Client | Option::Setting,
                            MAX_NUM_SERVER_ADDR_ITEMS,
                            StringVecOption::allValid,
                            [] ( VectorOption<QString>& ref, const QDomElement& xmlSection, QString& errmsg ) {
                                bool ret = ref.readXmlDefault (
                                    xmlSection,
                                    errmsg,
                                    [] ( VectorOption<QString>& refX, QList<QString> argsX, QString& successmsgX, QString& errmsgX ) {
                                        return refX.validator ( refX, { Option::stringFromBase64 ( argsX.takeFirst() ) }, successmsgX, errmsgX );
                                    } );
                                if ( !ret )
                                {
                                    return false;
                                }
                                if ( ref.isEmpty() )
                                {
                                    QList<QString> args;
                                    for ( int i = 0; i < MAX_NUM_SERVER_ADDR_ITEMS; i++ )
                                    {
                                        StringOption so = StringOption ( QString ( "directoryaddress%1" ).arg ( i ), ref.behaviours );
                                        if ( so.readXML ( xmlSection, errmsg ) && so.value.size() > 0 )
                                        {
                                            args.append ( so.value );
                                        }
                                    }
                                    if ( args.empty() )
                                    {
                                        // maybe an old format was used
                                        for ( int i = 0; i < MAX_NUM_SERVER_ADDR_ITEMS; i++ )
                                        {
                                            StringOption so = StringOption ( QString ( "centralservaddr%1" ).arg ( i ), ref.behaviours );
                                            if ( so.readXML ( xmlSection, errmsg ) && so.value.size() > 0 )
                                            {
                                                args.append ( so.value );
                                            }
                                        }
                                    }
                                    if ( args.empty() )
                                    {
                                        StringOption so = StringOption ( "centralservaddr", ref.behaviours );
                                        if ( so.readXML ( xmlSection, errmsg ) && so.value.size() > 0 )
                                        {
                                            args.append ( so.value );
                                        }
                                    }
                                    if ( args.size() > 0 )
                                    {
                                        QString msg;
                                        if ( !ref.validator ( ref, { args.join ( "\n" ) }, msg, errmsg ) )
                                        {
                                            return false;
                                        }
                                    }
                                }
                                return true;
                            } ),

    // Window positions stored as base64 string ----------------------------
    // set from storage with fromBase64String(...)
    // use toBase64String() to get the string for storage
    bo_winposmain ( "winposmain", Option::Common | Option::Setting ),
    bo_winposset ( "winposset", Option::Client | Option::Setting ),
    bo_winposchat ( "winposchat", Option::Client | Option::Setting ),
    bo_winposcon ( "winposcon", Option::Client | Option::Setting ),

    // Window visibility settings ------------------------------------------
    fo_winvisset ( "winvisset", Option::Client | Option::Setting, FlagOption::allValid ),
    fo_winvischat ( "winvischat", Option::Client | Option::Setting, FlagOption::allValid ),
    fo_winviscon ( "winviscon", Option::Client | Option::Setting, FlagOption::allValid ),

    // Settings tab --------------------------------------------------------
    io_settingstab ( "settingstab", Option::Client | Option::Setting, 0, 0, 2 ),

    // All mixer setting used "name%i" (or "name%i_base64" for tag) for storage
    // Mixer settings: tags stored as base64 string ------------------------
    // set from storage with fromBase64String(...)
    // use toBase64String() to get the string for storage
    // I'd expect there to be a max len?
    vo_storedfadertag ( "storedfadertag",
                        Option::Client | Option::Setting,
                        MAX_NUM_STORED_FADER_SETTINGS,
                        StringVecOption::allValid,
                        [] ( VectorOption<QString>& ref, const QDomElement& xmlSection, QString& errmsg ) {
                            bool ret = ref.readXmlDefault (
                                xmlSection,
                                errmsg,
                                [] ( VectorOption<QString>& refX, QList<QString> argsX, QString& successmsgX, QString& errmsgX ) {
                                    return refX.validator ( refX, { Option::stringFromBase64 ( argsX.takeFirst() ) }, successmsgX, errmsgX );
                                } );
                            if ( !ret )
                            {
                                return false;
                            }
                            if ( ref.isEmpty() )
                            {
                                // maybe the old format was used
                                QList<QString> args;
                                for ( int i = 0; i < MAX_NUM_STORED_FADER_SETTINGS; i++ )
                                {
                                    StringOption so =
                                        StringOption ( QString ( "%1%2_base64" ).arg ( ref.name ).arg ( i ), ref.behaviours | Option::IsBase64 );
                                    if ( so.readXML ( xmlSection, errmsg ) && so.value.size() > 0 )
                                    {
                                        args.append ( so.value );
                                    }
                                }
                                if ( args.size() > 0 )
                                {
                                    QString msg;
                                    if ( !ref.validator ( ref, { args.join ( "\n" ) }, msg, errmsg ) )
                                    {
                                        return false;
                                    }
                                }
                            }
                            return true;
                        } ),

    // Mixer settings: fader levels ----------------------------------------
    // stored values must be 0 to AUD_MIX_FADER_MAX
    vo_storedfaderlevel ( "storedfaderlevel",
                          Option::Client | Option::Setting,
                          MAX_NUM_STORED_FADER_SETTINGS,
                          100,
                          IntVecOption::allValid,
                          [] ( VectorOption<int>& ref, const QDomElement& xmlSection, QString& errmsg ) {
                              bool ret = ref.readXmlDefault (
                                  xmlSection,
                                  errmsg,
                                  [] ( VectorOption<int>& refX, QList<QString> argsX, QString& successmsgX, QString& errmsgX ) {
                                      return refX.validator ( refX, { Option::stringFromBase64 ( argsX.takeFirst() ) }, successmsgX, errmsgX );
                                  } );
                              if ( !ret )
                              {
                                  return false;
                              }
                              if ( ref.isEmpty() )
                              {
                                  // maybe the old format was used
                                  QList<QString> args;
                                  for ( int i = 0; i < MAX_NUM_SERVER_ADDR_ITEMS; i++ )
                                  {
                                      StringOption so = StringOption ( QString ( "%1%2" ).arg ( ref.name ).arg ( i ), ref.behaviours );
                                      if ( so.readXML ( xmlSection, errmsg ) && so.value.size() > 0 )
                                      {
                                          args.append ( so.value );
                                      }
                                  }
                                  if ( args.size() > 0 )
                                  {
                                      QString msg;
                                      if ( !ref.validator ( ref, { args.join ( "\n" ) }, msg, errmsg ) )
                                      {
                                          return false;
                                      }
                                  }
                              }
                              return true;
                          } ),

    // Mixer settings: pan values ------------------------------------------
    // stored values must be 0 to AUD_MIX_PAN_MAX
    vo_storedpanvalue ( "storedpanvalue",
                        Option::Client | Option::Setting,
                        MAX_NUM_STORED_FADER_SETTINGS,
                        50,
                        IntVecOption::allValid,
                        [] ( VectorOption<int>& ref, const QDomElement& xmlSection, QString& errmsg ) {
                            bool ret = ref.readXmlDefault (
                                xmlSection,
                                errmsg,
                                [] ( VectorOption<int>& refX, QList<QString> argsX, QString& successmsgX, QString& errmsgX ) {
                                    return refX.validator ( refX, { Option::stringFromBase64 ( argsX.takeFirst() ) }, successmsgX, errmsgX );
                                } );
                            if ( !ret )
                            {
                                return false;
                            }
                            if ( ref.isEmpty() )
                            {
                                QList<QString> args;
                                for ( int i = 0; i < MAX_NUM_SERVER_ADDR_ITEMS; i++ )
                                {
                                    StringOption so   = StringOption ( QString ( "%1%2" ).arg ( ref.name ).arg ( i ), ref.behaviours );
                                    bool         retX = so.readXML ( xmlSection, errmsg );
                                    if ( retX && so.value.size() > 0 )
                                    {
                                        args.append ( so.value );
                                    }
                                }
                                if ( args.size() > 0 )
                                {
                                    QString msg;
                                    if ( !ref.validator ( ref, { args.join ( "\n" ) }, msg, errmsg ) )
                                    {
                                        return false;
                                    }
                                }
                            }
                            return true;
                        } ),

    // Mixer settings: solos -----------------------------------------------
    vo_storedfaderissolo ( "storedfaderissolo",
                           Option::Client | Option::Setting,
                           MAX_NUM_STORED_FADER_SETTINGS,
                           0,
                           FlagVecOption::allValid,
                           [] ( VectorOption<bool>& ref, const QDomElement& xmlSection, QString& errmsg ) {
                               bool ret = ref.readXmlDefault (
                                   xmlSection,
                                   errmsg,
                                   [] ( VectorOption<bool>& refX, QList<QString> argsX, QString& successmsgX, QString& errmsgX ) {
                                       return refX.validator ( refX, { Option::stringFromBase64 ( argsX.takeFirst() ) }, successmsgX, errmsgX );
                                   } );
                               if ( !ret )
                               {
                                   return false;
                               }
                               if ( ref.isEmpty() )
                               {
                                   // maybe the old format was used
                                   QList<QString> args;
                                   for ( int i = 0; i < MAX_NUM_STORED_FADER_SETTINGS; i++ )
                                   {
                                       StringOption so = StringOption ( QString ( "%1%2" ).arg ( ref.name ).arg ( i ), ref.behaviours );
                                       if ( so.readXML ( xmlSection, errmsg ) && so.value.size() > 0 )
                                       {
                                           args.append ( so.value );
                                       }
                                   }
                                   if ( args.size() > 0 )
                                   {
                                       QString msg;
                                       if ( !ref.validator ( ref, { args.join ( "\n" ) }, msg, errmsg ) )
                                       {
                                           return false;
                                       }
                                   }
                               }
                               return true;
                           } ),

    // Mixer settings: mutes -----------------------------------------------
    vo_storedfaderismute ( "storedfaderismute",
                           Option::Client | Option::Setting,
                           MAX_NUM_STORED_FADER_SETTINGS,
                           0,
                           FlagVecOption::allValid,
                           [] ( VectorOption<bool>& ref, const QDomElement& xmlSection, QString& errmsg ) {
                               bool ret = ref.readXmlDefault (
                                   xmlSection,
                                   errmsg,
                                   [] ( VectorOption<bool>& refX, QList<QString> argsX, QString& successmsgX, QString& errmsgX ) {
                                       return refX.validator ( refX, { Option::stringFromBase64 ( argsX.takeFirst() ) }, successmsgX, errmsgX );
                                   } );
                               if ( !ret )
                               {
                                   return false;
                               }
                               if ( ref.isEmpty() )
                               {
                                   // maybe the old format was used
                                   QList<QString> args;
                                   for ( int i = 0; i < MAX_NUM_STORED_FADER_SETTINGS; i++ )
                                   {
                                       StringOption so = StringOption ( QString ( "%1%2" ).arg ( ref.name ).arg ( i ), ref.behaviours );
                                       if ( so.readXML ( xmlSection, errmsg ) && so.value.size() > 0 )
                                       {
                                           args.append ( so.value );
                                       }
                                   }
                                   if ( args.size() > 0 )
                                   {
                                       QString msg;
                                       if ( !ref.validator ( ref, { args.join ( "\n" ) }, msg, errmsg ) )
                                       {
                                           return false;
                                       }
                                   }
                               }
                               return true;
                           } ),

    // Mixer settings: mutes -----------------------------------------------
    // stored values must be INVALID_INDEX to MAX_NUM_FADER_GROUPS - 1
    vo_storedgroupid ( "storedgroupid",
                       Option::Client | Option::Setting,
                       MAX_NUM_STORED_FADER_SETTINGS,
                       -1,
                       IntVecOption::allValid,
                       [] ( VectorOption<int>& ref, const QDomElement& xmlSection, QString& errmsg ) {
                           bool ret = ref.readXmlDefault (
                               xmlSection,
                               errmsg,
                               [] ( VectorOption<int>& refX, QList<QString> argsX, QString& successmsgX, QString& errmsgX ) {
                                   return refX.validator ( refX, { Option::stringFromBase64 ( argsX.takeFirst() ) }, successmsgX, errmsgX );
                               } );
                           if ( !ret )
                           {
                               return false;
                           }
                           if ( ref.isEmpty() )
                           {
                               QList<QString> args;
                               for ( int i = 0; i < MAX_NUM_SERVER_ADDR_ITEMS; i++ )
                               {
                                   StringOption so = StringOption ( QString ( "%1%2" ).arg ( ref.name ).arg ( i ), ref.behaviours );
                                   if ( so.readXML ( xmlSection, errmsg ) && so.value.size() > 0 )
                                   {
                                       args.append ( so.value );
                                   }
                               }
                               if ( args.size() > 0 )
                               {
                                   QString msg;
                                   if ( !ref.validator ( ref, { args.join ( "\n" ) }, msg, errmsg ) )
                                   {
                                       return false;
                                   }
                               }
                           }
                           return true;
                       } )
{
    // Further fine tuning
    so_serverinfo.xmlWriter = [] ( StringOption& ref, QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg ) {
        QString xmlText = ( ref.behaviours & Option::IsBase64 ) ? ref.toBase64String() : ref.value;
        return ref.value == QString ( ";;" ) || ref.value == ref.defvalue ||
               TOption<QString>::writeXmlDefault ( ref, xmlDoc, xmlSection, xmlText, errmsg );
    };
}

// These are the instances - all else are mere references
struct JamulusOptions AllOptions;

// All of the command line options are viable settings except Exit and Early options
// plus everything else
AnyOptionList settings = AnyOptionList{
    AllOptions.io_port,
    AllOptions.io_qos,
    AllOptions.so_language,
    AllOptions.fo_notranslation,
    AllOptions.fo_ipv6,
    AllOptions.io_jsonrpcport,
    AllOptions.so_jsonrpcsecret,
    AllOptions.so_jsonrpcbindip,

    AllOptions.fo_discononquit,
    AllOptions.so_directory,
    AllOptions.so_directoryDeprecated,
    AllOptions.so_serverlistfile,
    AllOptions.vs_serverlistfilter,
    AllOptions.fo_fastupdate,
    AllOptions.so_logfile,
    AllOptions.fo_licence,
    AllOptions.so_htmlstatus,
    AllOptions.so_serverinfo,
    AllOptions.so_serverpublicip,
    AllOptions.fo_delaypan,
    AllOptions.so_recording,
    AllOptions.fo_norecord,
    AllOptions.fo_server,
    AllOptions.so_serverbindip,
    AllOptions.fo_multithreading,
    AllOptions.io_numchannels,
    AllOptions.so_welcomemessage,
    AllOptions.fo_startminimized,

    AllOptions.so_connect,
    AllOptions.fo_nojackconnect,
    AllOptions.fo_mutestream,
    AllOptions.fo_mutemyown,
    AllOptions.so_clientname,
    AllOptions.so_ctrlmidich,
    AllOptions.fo_audioalerts,
    AllOptions.fo_showallservers,
    AllOptions.fo_showanalyzerconsole,

    AllOptions.vo_serveraddresses,
    AllOptions.io_newclientlevel,
    AllOptions.io_inputboost,
    AllOptions.fo_detectfeedback,
    AllOptions.fo_connectdlgshowallmusicians,
    AllOptions.io_channelsort,
    AllOptions.fo_ownfaderfirst,
    AllOptions.io_numrowsmixpan,
    AllOptions.so_name,
    AllOptions.io_instrument,
    AllOptions.io_country,
    AllOptions.so_city,
    AllOptions.io_skill,
    AllOptions.io_audfad,
    AllOptions.io_revlev,
    AllOptions.fo_reverblchan,
    AllOptions.so_auddev,
    AllOptions.io_sndcrdinlch,
    AllOptions.io_sndcrdinrch,
    AllOptions.io_sndcrdoutlch,
    AllOptions.io_sndcrdoutrch,
    AllOptions.io_prefsndcrdbufidx,
    AllOptions.io_audiochannels,
    AllOptions.io_audioquality,
    AllOptions.fo_enableopussmall,
    AllOptions.fo_autojitbuf,
    AllOptions.io_jitbuf,
    AllOptions.io_jitbufserver,
    AllOptions.io_guidesign,
    AllOptions.io_meterstyle,
    AllOptions.io_directorytype,
    AllOptions.io_customdirectoryindex,
    AllOptions.vo_directoryaddresses,
    AllOptions.bo_winposmain,
    AllOptions.bo_winposset,
    AllOptions.bo_winposchat,
    AllOptions.bo_winposcon,
    AllOptions.fo_winvisset,
    AllOptions.fo_winvischat,
    AllOptions.fo_winviscon,
    AllOptions.io_settingstab,
};

// Because there's a menu option for these, they're split out
AnyOptionList faderSettings = AnyOptionList{
    AllOptions.vo_storedfadertag,
    AllOptions.vo_storedfaderlevel,
    AllOptions.vo_storedpanvalue,
    AllOptions.vo_storedfaderissolo,
    AllOptions.vo_storedfaderismute,
    AllOptions.vo_storedgroupid,
};

bool AnyOption::ProcessArgs ( int argc, char** argv, QString& errmsg )
{
    // The AllOptions is the real place the wrapper to the TOption exists.
    // This is _just_ for command line options, as a list makes things easier.
    AnyOptionList options = {
        AllOptions.fo_help,
        AllOptions.fo_helphtml,
        AllOptions.fo_version,

        AllOptions.fo_quiet,
        AllOptions.so_inifile,
        AllOptions.fo_nogui,

        AllOptions.io_port,
        AllOptions.io_qos,
        AllOptions.so_language,
        AllOptions.fo_notranslation,
        AllOptions.fo_ipv6,
        AllOptions.io_jsonrpcport,
        AllOptions.so_jsonrpcsecret,
        AllOptions.so_jsonrpcbindip,

        AllOptions.fo_discononquit,
        AllOptions.so_directory,
        AllOptions.so_directoryDeprecated,
        AllOptions.so_serverlistfile,
        AllOptions.vs_serverlistfilter,
        AllOptions.fo_fastupdate,
        AllOptions.so_logfile,
        AllOptions.fo_licence,
        AllOptions.so_htmlstatus,
        AllOptions.so_serverinfo,
        AllOptions.so_serverpublicip,
        AllOptions.fo_delaypan,
        AllOptions.so_recording,
        AllOptions.fo_norecord,
        AllOptions.fo_server,
        AllOptions.so_serverbindip,
        AllOptions.fo_multithreading,
        AllOptions.io_numchannels,
        AllOptions.so_welcomemessage,
        AllOptions.fo_startminimized,

        AllOptions.so_connect,
        AllOptions.fo_nojackconnect,
        AllOptions.fo_mutestream,
        AllOptions.fo_mutemyown,
        AllOptions.so_clientname,
        AllOptions.so_ctrlmidich,
        AllOptions.fo_showallservers,
        AllOptions.fo_showanalyzerconsole,
    };

    QStringList optionArgs;
    for ( int argn = 1; argn < argc; argn++ )
    {
        optionArgs += QString ( argv[argn] );
    }

    QList<AnyOption*> parsedOptions;
    QList<AnyOption*> ServerOnlyOptions;
    QList<AnyOption*> ClientOnlyOptions;

    // parse the options that cause exit without further processing first
    // NOTE: Exit and Early options MUST NOT check the quiet flag
    if ( !parse ( optionArgs, options, parsedOptions, Option::Exits, true, errmsg ) )
    {
        return false;
    }

    // std::cout here to avoid stderr, which qInfo() uses...
    if ( AllOptions.fo_help.value )
    {
        std::cout << qUtf8Printable ( AnyOption::usageArguments ( argv[0], options ) ) << "\n";
        exit ( 0 );
    }

    if ( AllOptions.fo_helphtml.value )
    {
        std::cout << qUtf8Printable ( AnyOption::usageArgumentsHTML ( options ) ) << "\n";
        exit ( 0 );
    }

    if ( AllOptions.fo_version.value )
    {
        std::cout << qUtf8Printable ( GetVersionAndNameStr ( false ) ) << "\n";
        exit ( 0 );
    }

    // next, options that affect processing the inifile
    // NOTE: Exit and Early options MUST NOT check the quiet flag
    if ( !parse ( optionArgs, options, parsedOptions, Option::Early, true, errmsg ) )
    {
        return false;
    }

    bool& quiet  = AllOptions.fo_quiet.value;
    bool& nogui  = AllOptions.fo_nogui.value;
    bool& server = AllOptions.fo_server.value;

    // Some build-specific overrides and checks
#if defined( Q_OS_MACX )
#    if defined( SERVER_BUNDLE )
    if ( !quiet )
    {
        qInfo() << "MacOS Server bundle.";
    }
    allOptions.fo_server.value = server = true;
#    else
    if ( !quiet )
    {
        qInfo() << "MacOS Client bundle.";
    }
    allOptions.fo_server.value = server = false;
#    endif
#endif

#ifdef HEADLESS
    if ( !nogui )
    {
        qWarning() << "This build has no GUI support. Running in headless mode.";
        allOptions.fo_nogui.value = nogui = true;
    }
#else
#    if defined( Q_OS_IOS )
    if ( nogui )
    {
        qWarning() << "This iOS build does not support headless mode.";
        allOptions.fo_nogui.value = nogui = false;
    }
    if ( server )
    {
        // Client only - TODO: maybe a switch in interface to exit and rerun with --server? ?!?!
        qWarning() << "This iOS build does not support server mode.";
        allOptions.fo_server.value = server = false;
    }
#    endif
#endif

#ifdef SERVER_ONLY
    // yes, it's inconsistent with HEADLESS...
    if ( !server )
    {
        errmsg = "This is a server-only build.  Did you mean to run with '--server'?";
        return false;
    }
#endif

    if ( !quiet )
    {
        QList<QString> args;
        QString        successmsg;

        // welcome to Jamulus ... maybe we want a shorter one, too?
        std::cout << qUtf8Printable ( GetVersionAndNameStr ( false ) ) << "\n";

        // Exits and Early options were parsed quietly.
        // Only nogui and server need announcing, currently.
#ifndef HEADLESS
        if ( AnyOption::parsedOptionsContains ( parsedOptions, AllOptions.fo_nogui ) )
        {
            AllOptions.fo_nogui.validator ( AllOptions.fo_nogui, args, successmsg, errmsg );
            std::cout << qUtf8Printable ( successmsg ) << "\n";
        }
#endif
        if ( AnyOption::parsedOptionsContains ( parsedOptions, AllOptions.fo_server ) )
        {
            AllOptions.fo_server.validator ( AllOptions.fo_server, args, successmsg, errmsg );
            std::cout << qUtf8Printable ( successmsg ) << "\n";
        }
    }

    // Different value for client and server...  This must happen before loading the inifile or further parsing
    if ( AllOptions.fo_server.value )
    {
        AllOptions.io_directorytype.value = AllOptions.io_directorytype.defvalue = AllOptions.io_directorytype.minvalue = AT_NONE;
    }

    // loadSettings sets the actual file name in inifile, for use when saving
    if ( !loadSettings ( server, nogui, parsedOptions, quiet, errmsg ) )
    {
        return false;
    }

    // now parse the remainder of the command line, overriding any loaded values
    if ( !parse ( optionArgs, options, parsedOptions, Option::Common, quiet, errmsg ) )
    {
        return false;
    }

    if ( !parse ( optionArgs, options, ServerOnlyOptions, Option::Server, quiet, errmsg ) )
    {
        return false;
    }

    // Treat client-only options as understood even in server-only _build_ - fail later
    if ( !parse ( optionArgs, options, ClientOnlyOptions, Option::Client, quiet, errmsg ) )
    {
        return false;
    }

    bool literal = false;
    if ( optionArgs.size() > 0 && "--" == optionArgs[0] )
    {
        optionArgs.takeFirst();
        literal = true;
    }

#ifndef SERVER_ONLY
    // finally, for client where no connect value already set and there's a trailing non-option arg, use it
    if ( !server && optionArgs.size() > 0 && !optionArgs[0].isEmpty() && ( literal || !optionArgs[0].startsWith ( "-" ) ) )
    {
        // if the --connect option has already been used explicitly, skip (and drop into syntax error block)
        if ( AllOptions.so_connect.value == AllOptions.so_connect.defvalue )
        {
            AllOptions.so_connect.value = optionArgs.takeFirst();
            if ( !quiet )
            {
                std::cout << qUtf8Printable ( QString ( AllOptions.so_connect.validText ).arg ( AllOptions.so_connect.value ) ) << "\n";
            }
        }
    }
#endif

    // optionArgs should be empty
    if ( !optionArgs.empty() )
    {
        errmsg = QString ( "Syntax error: unused trailing options. See '%1 --help' for help. (%2)" ).arg ( APP_NAME, optionArgs.join ( " " ) );
        return false;
    }

    if ( server )
    {
        if ( !ClientOnlyOptions.empty() )
        {
            errmsg = QString ( "%1: Server mode requested but client only option(s) used.  See '--help' for help" ).arg ( argv[0] );
            return false;
        }

        parsedOptions.append ( ServerOnlyOptions );
        if ( !checkServerOptions ( parsedOptions, errmsg ) )
        {
            return false;
        }
    }
#ifndef SERVER_ONLY
    else
    {
        if ( !ServerOnlyOptions.empty() )
        {
            errmsg = QString ( "%1: Server only option(s) used.  Did you omit '--server'?  See '--help' for help" ).arg ( argv[0] );
            return false;
        }

        parsedOptions.append ( ClientOnlyOptions );
        if ( !checkClientOptions ( parsedOptions, errmsg ) )
        {
            return false;
        }
    }
#endif

#ifndef HEADLESS
    // if notranslation is set on the command line, any non-default language in settings is overridden.  Let the user know.
    // needs to be after settings loaded and command line options parsed.
    // (Only applies to GUI.)
    // And I still don't see why I added it.
    // (It can't go into settings or parse, though - it's only when there's a conflict and I don't want to add an ordering dependency.)
    if ( AnyOption::parsedOptionsContains ( parsedOptions, AllOptions.fo_notranslation ) && !AllOptions.fo_nogui.value &&
         AllOptions.so_language.value != AllOptions.so_language.defvalue )
    {
        if ( !quiet )
        {
            qInfo() << qUtf8Printable (
                QString ( "%1 option used, setting UI language to %2 and not %3." )
                    .arg ( AllOptions.fo_notranslation.longOptions[0], AllOptions.so_language.defvalue, AllOptions.so_language.value ) );
        }
    }
#endif

#ifndef NO_JSON_RPC
    // If JSON-RPC server enabled, check a secret is supplied
    if ( AllOptions.io_jsonrpcport.value != INVALID_PORT && AllOptions.so_jsonrpcsecret.value.isEmpty() )
    {
        errmsg = "JSON-RPC server is enabled but no secret supplied.  Please use --jsonrpcsecretfile to name the file with the secret.";
        return false;
    }

    // If JSON-RPC server not enabled, check no bind address is supplied
    std::cout << qUtf8Printable ( QString ( "allOptions.so_jsonrpcbindip.value='%1'" ).arg ( AllOptions.so_jsonrpcbindip.value ) ) << "\n";
    if ( AllOptions.io_jsonrpcport.value == INVALID_PORT && AnyOption::parsedOptionsContains ( parsedOptions, AllOptions.so_jsonrpcbindip ) )
    {
        errmsg = "JSON-RPC server is not enabled.  --jsonrpcbindip should not be used.";
        return false;
    }
#endif

    return true;
}

/*
 * Check chosen combinations of server options are valid
 */
bool AnyOption::checkServerOptions ( QList<AnyOption*>& parsedOptions, QString& errmsg )
{
    EDirectoryType directoryType = static_cast<EDirectoryType> ( AllOptions.io_directorytype.value );

    // deprecated settings map to current if current not given
    StringOption& directory = AllOptions.so_directory;
    if ( AnyOption::parsedOptionsContains ( parsedOptions, AllOptions.so_directoryDeprecated ) )
    {
        if ( !AnyOption::parsedOptionsContains ( parsedOptions, directory ) )
        {
            qWarning() << qUtf8Printable (
                QString ( "%1 options are deprecated and will be removed in a future version. "
                          "Instead, please use %2" )
                    .arg ( QStringList ( { AllOptions.so_directoryDeprecated.shortOptions.join ( ", " ),
                                           AllOptions.so_directoryDeprecated.longOptions.join ( ", " ) } )
                               .join ( ", " ),
                           QStringList ( { directory.shortOptions.join ( ", " ), directory.longOptions.join ( ", " ) } ).join ( ", " ) ) );
            directory.value                         = AllOptions.so_directoryDeprecated.value;
            AllOptions.so_directoryDeprecated.value = QString();
            AnyOption ao_directory                  = AnyOption ( directory );
            parsedOptions.append ( &ao_directory );
            parsedOptions.removeAt ( AnyOption::parsedOptionsIndexOf ( parsedOptions, AllOptions.so_directoryDeprecated ) );
        }
        else
        {
            errmsg = qUtf8Printable (
                QString ( "Specifying both %1 and %2 is unsupported " )
                    .arg ( QStringList ( { directory.shortOptions.join ( ", " ), directory.longOptions.join ( ", " ) } ).join ( ", " ),
                           QStringList ( { AllOptions.so_directoryDeprecated.shortOptions.join ( ", " ),
                                           AllOptions.so_directoryDeprecated.longOptions.join ( ", " ) } )
                               .join ( ", " ) ) );
            return false;
        }
    }

    // With ini file loaded, some care is required here regarding the directory type
    // - directory type may be AT_NONE, in which case any directory value is ignored
    // - directory type must be AT_CUSTOM (current default), to be running as a directory

    // With ini file loaded, if the --directory was specified, use AT_CUSTOM - user can change it back in GUI if they don't like it
    if ( AnyOption::parsedOptionsContains ( parsedOptions, directory ) )
    {
        if ( directoryType != AT_CUSTOM )
        {
            directoryType                     = AT_CUSTOM;
            AllOptions.io_directorytype.value = static_cast<qint64> ( directoryType );
            qWarning() << "Directory type set to Custom because command line directory specified.";
        }
    }

    // With Settings loaded, "Am I a directory?" becomes
    // - AT_CUSTOM set for chosen directory, directory value set to local host

    StringOption& serverListFile = AllOptions.so_serverlistfile;
    if ( directoryType != AT_CUSTOM ||
         ( directory.value.compare ( "localhost", Qt::CaseInsensitive ) != 0 && directory.value.compare ( "127.0.0.1" ) != 0 ) )
    {
        if ( !serverListFile.value.isEmpty() )
        {
            qWarning() << "Server list persistence file will only take effect when running as a directory.";
            serverListFile.value = QString();
        }

        StringVecOption& serverListFilter = AllOptions.vs_serverlistfilter;
        if ( serverListFilter.value.size() > 0 )
        {
            qWarning() << "Server list filter will only take effect when running as a directory.";
            serverListFilter.value.clear();
        }
    }
    else
    {
        if ( !serverListFile.value.isEmpty() )
        {
            QFile strServerListFile ( serverListFile.value );
            if ( !strServerListFile.open ( QFile::OpenModeFlag::ReadWrite ) )
            {
                errmsg = QString ( "Cannot create %1 for reading and writing.  Please check permissions." ).arg ( serverListFile.value );
                return false;
            }
        }
    }

    // With Settings loaded, "Am I a registered?" becomes
    // - AT_NONE not for chosen directory
    // - if AT_CUSTOM chosen, directory is set
    // - we need to check that here...

    if ( directoryType == AT_NONE || ( directoryType == AT_CUSTOM && directory.value.isEmpty() ) )
    {
        StringOption& serverPublicIP = AllOptions.so_serverpublicip;
        if ( !serverPublicIP.value.isEmpty() )
        {
            qInfo() << "Server Public IP will only take effect when registering a server with a directory.";
            // serverPublicIP.value = QString();
        }
    }

    if ( !AllOptions.so_logfile.value.isEmpty() )
    {
        QFile logFile ( AllOptions.so_logfile.value );
        if ( !logFile.open ( QFile::OpenModeFlag::ReadWrite ) )
        {
            errmsg = QString ( "Cannot create %1 for reading and writing.  Please check permissions." ).arg ( AllOptions.so_logfile.value );
            return false;
        }
    }

    if ( !AllOptions.so_htmlstatus.value.isEmpty() )
    {
        QFile htmlStatusFile ( AllOptions.so_htmlstatus.value );
        if ( !htmlStatusFile.open ( QFile::OpenModeFlag::ReadWrite ) )
        {
            errmsg = QString ( "Cannot create %1 for reading and writing.  Please check permissions." ).arg ( AllOptions.so_htmlstatus.value );
            return false;
        }
    }

    // consider checking so_recording directory - if missing attempt to create it now

    return true;
}

#ifndef SERVER_ONLY
/*
 * Check chosen combinations of client options are valid
 */
bool AnyOption::checkClientOptions ( QList<AnyOption*>& parsedOptions, QString& errmsg )
{
    // mute my own signal in personal mix is only supported for headless mode
    // Why?  Should this restriction will be removed..?  Maybe the feature should be removed?
    if ( !AllOptions.fo_nogui.value && AllOptions.fo_mutemyown.value )
    {
        qWarning() << "Mute my own signal in my personal mix is only supported in headless mode.";
        AllOptions.fo_mutemyown.value = false;
    }

    // adjust default port number (i.e. do not override if explicitly set) for client: use different default port than the server since
    // if the client is started before the server, the server would get a socket bind error
    if ( !AnyOption::parsedOptionsContains ( parsedOptions, AllOptions.io_port ) )
    {
        AllOptions.io_port.value += 10; // increment by 10
        if ( !AllOptions.fo_quiet.value )
        {
            std::cout << qUtf8Printable ( QString ( AllOptions.io_port.validText ).arg ( AllOptions.io_port.value ) ) << "\n";
        }
    }

    if ( AllOptions.fo_nogui.value && ( AllOptions.fo_showallservers.value || AllOptions.fo_showanalyzerconsole.value ) )
    {
        errmsg = qUtf8Printable ( QString ( "Cannot use %1 or %2 without the GUI" )
                                      .arg ( AllOptions.fo_showallservers.longOptions[0], AllOptions.fo_showanalyzerconsole.longOptions[0] ) );
        return false;
    }

    return true;
}
#endif

bool AnyOption::loadSettings ( bool server, bool nogui, QList<AnyOption*>& parsedOptions, bool quiet, QString& errmsg )
{
    bool parsedOptionsContainsIniFile = parsedOptionsContains ( parsedOptions, AllOptions.so_inifile );
#if defined( SERVER_ONLY ) && defined( HEADLESS )
    Q_UNUSED ( server )
    Q_UNUSED ( nogui )
    Q_UNUSED ( quiet )
    Q_UNUSED ( errmsg )
    if ( parsedOptionsContainsIniFile )
    {
        qWarning() << "** No initialization file support in headless server mode.";
    }
    return true; // do not load settings, even if a file is specified (but why?!)
#else
    // work out, based on server and nogui, whether we load setting and what the file name should be if it isn't set
    if ( server && nogui )
    {
        // the inifile is not supported for the headless server mode
        if ( parsedOptionsContainsIniFile )
        {
            qWarning() << "** No initialization file support in headless server mode.";
        }
        return true; // do not load settings, even if a file is specified (but why?!)
    }

    // If the user specified a file, use that (it's a valid name)
    if ( !parsedOptionsContainsIniFile && AllOptions.so_inifile.value == AllOptions.so_inifile.defvalue )
    {
        if ( server )
        {
            // override the default
            AllOptions.so_inifile.value = "Jamulusserver.ini";
        }

        QString& inifile = AllOptions.so_inifile.value;

        QString strConfigDir = QFileInfo ( QSettings ( QSettings::IniFormat, QSettings::UserScope, APP_NAME, APP_NAME ).fileName() ).absolutePath();

        inifile = AllOptions.so_inifile.value = QString ( "%1/%2" ).arg ( strConfigDir ).arg ( inifile );

        AllOptions.so_inifile.value = inifile;
    }

    // StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg
    QString successmsg;
    if ( !AllOptions.so_inifile.validator ( AllOptions.so_inifile, QStringList ( AllOptions.so_inifile.value ), successmsg, errmsg ) )
    {
        return false;
    }

    // We parsed the argument quietly so now we've decided on a value, we can announce it
    if ( !quiet && parsedOptionsContainsIniFile )
    {
        std::cout << qUtf8Printable ( successmsg ) << "\n";
    }

    // Last chance before we try actually reading the file...
    if ( !QFile::exists ( QFileInfo ( AllOptions.so_inifile.value ).absoluteFilePath() ) )
    {
        if ( !quiet && parsedOptionsContainsIniFile )
        {
            qWarning() << qUtf8Printable (
                QString ( "** \"%1\" was not found.  It will be created when %2 closes." ).arg ( AllOptions.so_inifile.value ).arg ( APP_NAME ) );
        }
        return true;
    }

    QDomDocument XMLDocument;
    if ( !getXMLDocument ( AllOptions.so_inifile.value, XMLDocument, errmsg ) )
    {
        return false;
    }

    if ( !readSettings ( settings, XMLDocument, errmsg ) )
    {
        return false;
    }

    if ( AllOptions.fo_server.value )
    {
        return true;
    }

    return readSettings ( faderSettings, XMLDocument, errmsg );
#endif
}

bool AnyOption::getXMLDocument ( const QString strFileName, QDomDocument& domXMLDoc, QString& errmsg )
{
    QFile file ( strFileName );
    if ( !file.open ( QIODevice::ReadOnly ) )
    {
        errmsg = QString ( "Could not read file %1.  Check the right name has been used." ).arg ( strFileName );
        return false;
    }
    domXMLDoc.setContent ( QTextStream ( &file ).readAll(), false );
    file.close();

    return true;
}

bool AnyOption::readSettings ( AnyOptionList& options, const QDomDocument& domXMLDoc, QString& errmsg )
{
    QString     xmlSectionName = AllOptions.fo_server.value ? "server" : "client";
    QDomElement xmlSection     = domXMLDoc.firstChildElement ( xmlSectionName );
    // That we are here means the file exists and loaded okay
    if ( xmlSection.isNull() )
    {
        errmsg = QString ( "Could not read the %1 settings.  Check the inifile name." ).arg ( xmlSectionName );
        return false;
    }

    Option::Behaviours behaviours = Option::Common | ( AllOptions.fo_server.value ? Option::Server : Option::Client );

    for ( int iIdx = 0; iIdx < options.size(); iIdx++ )
    {
        AnyOption& ao     = options[iIdx];
        Option&    option = *ao.optionPtr();

        // skip anything that isn't an applicable setting
        if ( ( ( option.behaviours & Option::Setting ) == Option::None ) || ( ( option.behaviours & behaviours ) == Option::None ) )
        {
            continue;
        }

        if ( !option.readXML ( xmlSection, errmsg ) )
        {
            qWarning() << qUtf8Printable ( QString ( " ** Failed to read setting %1: %2" ).arg ( option.name ).arg ( errmsg ) );
        }
    }
    return true;
}

bool AnyOption::LoadFaderSettings ( const QString strFileName )
{
    QDomDocument XMLDocument;
    QString      errmsg;

    if ( !AnyOption::getXMLDocument ( strFileName, XMLDocument, errmsg ) )
    {
        return false;
    }

    return readSettings ( faderSettings, XMLDocument, errmsg );
}

bool AnyOption::saveSettings()
{
#if !defined( SERVER_ONLY ) && !defined( HEADLESS )
    if ( AllOptions.fo_server.value && AllOptions.fo_nogui.value )
    {
        return true;
    }

    QDomDocument XMLDocument = QDomDocument();
    QString      errmsg;

    QDomElement xmlSection = XMLDocument.createElement ( AllOptions.fo_server.value ? "server" : "client" );
    XMLDocument.appendChild ( xmlSection );

    // if this fails, it could just mean file not found, so we can create it
    //-- do not do this, create from scratch (void)getXMLDocument ( allOptions.so_inifile.value, XMLDocument, errmsg );

    if ( !writeSettings ( settings, XMLDocument, xmlSection, errmsg ) )
    {
        throw CGenErr ( errmsg );
    }

    if ( !AllOptions.fo_server.value )
    {
        if ( !writeSettings ( faderSettings, XMLDocument, xmlSection, errmsg ) )
        {
            throw CGenErr ( errmsg );
        }
    }

    if ( !putXMLDocument ( AllOptions.so_inifile.value, XMLDocument, errmsg ) )
    {
        throw CGenErr ( errmsg );
    }
#endif

    return true;
}

bool AnyOption::SaveFaderSettings ( const QString strFileName )
{
    QDomDocument XMLDocument = QDomDocument();
    QDomElement  xmlSection  = XMLDocument.createElement ( "client" );
    QString      errmsg;

    XMLDocument.appendChild ( xmlSection );

    if ( !writeSettings ( faderSettings, XMLDocument, xmlSection, errmsg ) )
    {
        throw CGenErr ( errmsg );
    }

    return putXMLDocument ( strFileName, XMLDocument, errmsg );
}

bool AnyOption::putXMLDocument ( const QString strFileName, const QDomDocument& domXMLDoc, QString& errmsg )
{
    QFile file ( strFileName );
    if ( !file.open ( QIODevice::WriteOnly ) )
    {
        errmsg = QString ( "Could not write to file %1.  Check the right name has been used." ).arg ( strFileName );
        return false;
    }
    QTextStream ( &file ) << domXMLDoc.toString();
    file.close();

    return true;
}

bool AnyOption::writeSettings ( AnyOptionList& options, QDomDocument& XMLDocument, QDomElement& xmlSection, QString& errmsg )
{
    Option::Behaviours behaviours = Option::Common | ( AllOptions.fo_server.value ? Option::Server : Option::Client );

    for ( int iIdx = 0; iIdx < options.size(); iIdx++ )
    {
        AnyOption& ao     = options[iIdx];
        Option&    option = *ao.optionPtr();

        // skip anything that isn't an applicable setting
        if ( ( ( option.behaviours & Option::Setting ) == Option::None ) || ( ( option.behaviours & behaviours ) == Option::None ) )
        {
            continue;
        }

        if ( !option.writeXML ( XMLDocument, xmlSection, errmsg ) )
        {
            errmsg = QString ( "Failed to write setting %1: %2" ).arg ( option.name ).arg ( errmsg );
            return false;
        }
    }
    return true;
}

inline int AnyOption::getAnyOption ( AnyOptionList& options, const QString& optionArg )
{
    for ( int j = 0; j < options.size(); j++ )
    {
        AnyOption& ao     = options[j];
        Option&    option = *ao.optionPtr();
        if ( option.shortOptions.contains ( optionArg ) || option.longOptions.contains ( optionArg ) )
        {
            return j;
        }
    }
    return -1;
}

bool AnyOption::parse ( QStringList&       optionArgs,
                        AnyOptionList&     options,
                        QList<AnyOption*>& parsedOptions,
                        Option::Behaviours behaviours,
                        bool               quiet,
                        QString&           errmsg )
{
    if ( options.size() < 1 )
    {
        errmsg = QString ( "%1 has failed, please raise a bug report (options is empty)" ).arg ( APP_NAME );
        return false;
    }

    bool result = true;

    for ( int i = 0; result && i < optionArgs.size() && optionArgs[i].startsWith ( "-" ) && "--" != optionArgs[i]; )
    {
        const QString& optionArg = optionArgs[i];

        int anyOptionIndex = getAnyOption ( options, optionArg );
        if ( anyOptionIndex < 0 )
        {
            errmsg = QString ( "%1: unknown option, exiting" ).arg ( optionArg );
            result = false;
            break;
        }
        AnyOption& ao     = options[anyOptionIndex];
        Option&    option = *ao.optionPtr();

        // Parse the additional args for the option
        QStringList args;

        if ( i + 1 > optionArgs.size() - option.nArgs )
        {
            errmsg = QString ( "%1: expected %2 argument(s), got %3" ).arg ( optionArg ).arg ( option.nArgs ).arg ( args.size() );
            result = false;
            break;
        }

        for ( int k = 0; k < option.nArgs; k++ )
        {
            args += optionArgs[i + 1 + k];
        }

        if ( args.size() != option.nArgs )
        {
            errmsg = QString ( "%1: expected %2 argument(s), got %3" ).arg ( optionArg ).arg ( option.nArgs ).arg ( args.size() );
            result = false;
            break;
        }

        // option not in requested behaviours: skip the option argument and its parameters and try next option
        if ( ( option.behaviours & behaviours ) == Option::None )
        {
            i += args.size() + 1;
            continue;
        }

        // consume the option argument and its parameters
        for ( int k = 0; k < 1 + args.size(); k++ )
        {
            optionArgs.removeAt ( i );
        }

        parsedOptions += &ao;

        switch ( ao.optionType() )
        {
        case AnyOption::AOType::OTString:
            result = validator<StringOption> ( *ao.stringOptionPtr(), args, errmsg, quiet );
            break;
        case AnyOption::AOType::OTFlag:
            result = validator<FlagOption> ( *ao.flagOptionPtr(), args, errmsg, quiet );
            break;
        case AnyOption::AOType::OTInt:
            result = validator<IntOption> ( *ao.intOptionPtr(), args, errmsg, quiet );
            break;
        case AnyOption::AOType::OTByteArray:
            result = validator<ByteArrayOption> ( *ao.byteArrayOptionPtr(), args, errmsg, quiet );
            break;
        case AnyOption::AOType::OTStringVec:
            result = validator<StringVecOption> ( *ao.stringVecOptionPtr(), args, errmsg, quiet );
            break;
        case AnyOption::AOType::OTFlagVec:
            result = validator<FlagVecOption> ( *ao.flagVecOptionPtr(), args, errmsg, quiet );
            break;
        case AnyOption::AOType::OTIntVec:
            result = validator<IntVecOption> ( *ao.intVecOptionPtr(), args, errmsg, quiet );
            break;
        case AnyOption::AOType::OTUnassigned:
            // this should NEVER happen - unsupported option type!
            errmsg = QString ( "%1 has failed, please raise a bug report (option %2)" ).arg ( APP_NAME, option.name );
            result = false;
            break;
        }
    }

    return result;
}

inline int AnyOption::parsedOptionsIndexOf ( QList<AnyOption*>& list, Option& option )
{
    QList<AnyOption*>::iterator item    = list.begin();
    int                         indexOf = -1;
    while ( item != list.end() )
    {
        indexOf++;
        if ( ( *item )->optionPtr() == &option )
        {
            return indexOf;
        }
        item++;
    }
    return -1;
}

inline bool AnyOption::parsedOptionsContains ( QList<AnyOption*>& list, Option& option ) { return parsedOptionsIndexOf ( list, option ) != -1; }

inline QString AnyOption::usageArguments ( QString appName, AnyOptionList& options )
{
    const QString helpTextHeader = QString (
                                       // clang-format off
                                 "Usage:\n"
                                 "    %1 [-h | -? | --help] | [-v | --version]\n"
                                 "    %1 [common option...] [-s | --server] [server option...]\n"
                                 "    %1 [common option...] [client option...] [[-c | --connect] address]\n"
                                       // clang-format on
                                       )
                                       .arg ( appName );
    const QString helpTextFooter = QString (
                                       // clang-format off
                                 "\n"
                                 "Examples:\n"
                                 "    %1 -s --inifile myinifile.ini\n"
                                 "    %1 jams.example.io:22534\n"
                                 "\n"
                                 "For more information and localized help see:\n"
                                 "https://jamulus.io/wiki/Command-Line-Options"
                                       // clang-format on
                                       )
                                       .arg ( appName );
    return QString (
#if defined( _WIN32 )
               "\n"
#endif
               "%1\n"
               "%2\n"
               "%3\n"
#if defined( _WIN32 )
               "\n"
#endif
               )
        .arg ( helpTextHeader, getOptionsAsHelpText ( options ), helpTextFooter );
}

inline QString AnyOption::usageArgumentsHTML ( AnyOptionList& options )
{
    const QString helpTextHeader = QString (
                                       // clang-format off
                                 "<h2>Usage:</h2>\n"
                                 "<dl>\n"
                                 "<dd><tt>%1 [-h | -? | --help] | [-v | --version]</tt></dd>\n"
                                 "<dd><tt>%1 [common option...] [-s | --server] [server option...]</tt></dd>\n"
                                 "<dd><tt>%1 [common option...] [client option...] [address]</tt></dd>\n"
                                 "</dl>\n"
                                       // clang-format on
                                       )
                                       .arg ( APP_NAME );
    const QString helpTextFooter = QString (
                                       // clang-format off
                                 "<dl>\n"
                                 "<dt>Examples:</dt>\n"
                                 "<dd><tt>%1 -s --inifile myinifile.ini</tt></dd>\n"
                                 "<dd><tt>%1 jams.example.io:22534</tt></dd>\n"
                                 "</dl>\n"
                                 "<p>For more information and localized help see the\n"
                                 "<a href=\"https://jamulus.io/wiki/Command-Line-Options\"><em>Command-Line-Options</em></a>\n"
                                 "wiki page.</p>"
                                       // clang-format on
                                       )
                                       .arg ( APP_NAME );
    return QString ( "%1\n"
                     "%2\n"
                     "%3\n" )
        .arg ( helpTextHeader, getOptionsAsHTML ( options ), helpTextFooter );
}

inline QString AnyOption::getOptionsAsHelpText ( AnyOptionList& options )
{
    return getFormattedtSectionsHeadings ( "%1:\n"
                                           "%%2\n" )
        .arg ( getOptionsAsHelpText ( options, Option::Exits ),
               getOptionsAsHelpText ( options, Option::Common ),
               getOptionsAsHelpText ( options, Option::Server ),
               getOptionsAsHelpText ( options, Option::Client ) );
}

inline QString AnyOption::getOptionsAsHTML ( AnyOptionList& options )
{
    return getFormattedtSectionsHeadings ( "<h3>%1:</h3>\n"
                                           "<dl>%%2\n"
                                           "<dd></dd>\n"
                                           "</dl>" )
        .arg ( getOptionsAsHTML ( options, Option::Exits ),
               getOptionsAsHTML ( options, Option::Common ),
               getOptionsAsHTML ( options, Option::Server ),
               getOptionsAsHTML ( options, Option::Client ) );
}

inline QString AnyOption::getFormattedtSectionsHeadings ( QString format )
{
    const QStringList sectionHeadings = { "Help and version", "Common options", "Server options", "Client options" };
    QStringList       formattedHeadings;

    for ( int i = 0; i < sectionHeadings.size(); i++ )
    {
        formattedHeadings += format.arg ( sectionHeadings[i] ).arg ( i );
    }

    return formattedHeadings.join ( "\n" );
}

inline QString AnyOption::getOptionsAsHelpText ( AnyOptionList& options, Option::Behaviours behaviours )
{
    return getOptionsAs ( options, behaviours, "  %1\n", "        %1" );
}

inline QString AnyOption::getOptionsAsHTML ( AnyOptionList& options, Option::Behaviours behaviours )
{
    return getOptionsAs ( options, behaviours, "<dt>%1</dt>\n", "<dd>%1<dd>" );
}

QString AnyOption::getOptionsAs ( AnyOptionList& options, Option::Behaviours behaviours, QString str1, QString str2 )
{
    QStringList sl;
    for ( int i = 0; i < options.size(); i++ )
    {
        Option& o = *options[i].optionPtr();
        if ( ( o.behaviours & behaviours ) == Option::None )
        {
            continue;
        }
        if ( o.helpText.isEmpty() || ( o.shortOptions.isEmpty() && o.longOptions.isEmpty() ) )
        {
            continue;
        }
        QString helpText;
        if ( !o.shortOptions.isEmpty() || !o.longOptions.isEmpty() )
        {
            QStringList op = o.shortOptions + o.longOptions;
            helpText += QString ( str1 ).arg ( op.join ( ", " ) );
        }
        helpText += QString ( str2 ).arg ( o.helpText );
        sl += helpText;
    }
    return sl.join ( "\n" );
}
