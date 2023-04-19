/******************************************************************************\
 * Copyright (c) 2021-2022
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
#pragma once

#include <iostream>
#include <QCoreApplication>
#include <QtCore/qhash.h>
#include <QtCore/qmap.h>
#include <QtCore/qpair.h>
#include <QtCore/qvector.h>
#include <QHostAddress>
#include <QDir>
#include <QDomDocument>
#include <QSettings>

#include "global.h"
#include "util.h"

class Option
{
public:
    enum Behaviour
    {
        None     = 0x00,
        Common   = 0x01,
        Client   = 0x02,
        Server   = 0x04,
        Early    = 0x08,
        Exits    = 0x10,
        Setting  = 0x20,
        IsBase64 = 0x40,
    };
    Q_DECLARE_FLAGS ( Behaviours, Behaviour )

    Option() {}

    Option ( Option& rhs )
    {
        name         = rhs.name;
        behaviours   = rhs.behaviours;
        shortOptions = rhs.shortOptions;
        longOptions  = rhs.longOptions;
        helpText     = rhs.helpText;
        validText    = rhs.validText;
        nArgs        = rhs.nArgs;
    }

    Option ( QString        nameX,
             Behaviours     behavioursX,
             QList<QString> shortOptionsX,
             QList<QString> longOptionsX,
             QString        helpTextX,
             QString        validTextX,
             int            nArgsX ) :
        name ( nameX ),
        behaviours ( behavioursX ),
        shortOptions ( shortOptionsX ),
        longOptions ( longOptionsX ),
        helpText ( helpTextX ),
        validText ( validTextX ),
        nArgs ( nArgsX )
    {}

    QString        name;
    Behaviours     behaviours;
    QList<QString> shortOptions;
    QList<QString> longOptions;
    QString        helpText;
    QString        validText;
    int            nArgs;

    virtual ~Option() {}

    virtual QString toString() { return QString(); }

    virtual bool readXML ( const QDomElement& xmlSection, QString& errmsg )
    {
        qInfo() << "Option::readXML(...)" << qUtf8Printable ( name );
        return true;
        Q_UNUSED ( xmlSection )
        Q_UNUSED ( errmsg )
    }
    virtual bool writeXML ( QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg )
    {
        qInfo() << "Option::writeXML(...)" << qUtf8Printable ( name );
        return true;
        Q_UNUSED ( xmlDoc )
        Q_UNUSED ( xmlSection )
        Q_UNUSED ( errmsg )
    }

    Option& operator= ( Option rhs )
    {
        name         = rhs.name;
        behaviours   = rhs.behaviours;
        shortOptions = rhs.shortOptions;
        longOptions  = rhs.longOptions;
        helpText     = rhs.helpText;
        validText    = rhs.validText;
        nArgs        = rhs.nArgs;
        return *this;
    }

    static inline QByteArray byteArrayFromBase64 ( const QString& base64String ) { return QByteArray::fromBase64 ( base64String.toLatin1() ); }
    static inline QString    stringFromBase64 ( const QString& base64String )
    {
        return QString::fromUtf8 ( QByteArray::fromBase64 ( base64String.toLatin1() ) );
    }
};
Q_DECLARE_OPERATORS_FOR_FLAGS ( Option::Behaviours )

template<typename TOption>
bool validator ( TOption& option, QList<QString>& args, QString& errmsg, bool quiet )
{
    QString successmsg = QString();
    bool    result     = option.validator ( option, args, successmsg, errmsg );

    if ( result && !quiet )
    {
        std::cout << qUtf8Printable ( successmsg != QString() ? successmsg
                                                              : QString ( "- %1 set to %2" ).arg ( option.name ).arg ( option.toString() ) )
                  << "\n";
    }
    return result;
}

template<typename T>
class TOption : public Option
{
public:
    TOption<T> ( const TOption<T>& to ) :
        Option ( to.name, to.behaviours, to.shortOptions, to.longOptions, to.helpText, to.validText, to.nArgs ),
        defvalue ( to.defvalue ),
        value ( to.value )
    {}
    TOption<T> ( QString        nameX,
                 Behaviours     behavioursX,
                 QList<QString> shortOptionsX,
                 QList<QString> longOptionsX,
                 QString        helpTextX,
                 QString        validTextX,
                 int            nArgsX,
                 T              defvalueX,
                 T              valueX ) :
        Option ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, nArgsX ),
        defvalue ( defvalueX ),
        value ( valueX )
    {}

    T defvalue;
    T value;

    ~TOption<T>() override {}

    TOption<T>& operator= ( TOption<T> rhs )
    {
        Option::operator= ( rhs );
        defvalue = rhs.defvalue;
        value    = rhs.value;
        return *this;
    }

    static inline bool readXmlDefault ( TOption<T>& to, const QDomElement& xmlSection, QString& xmlText, QString& )
    {
        QDomElement xmlKey = xmlSection.firstChildElement ( to.name );

        if ( xmlKey.isNull() && ( to.behaviours & Option::IsBase64 ) != 0 && !to.name.endsWith ( "_base64" ) )
        {
            // try with "_base64" on the name
            xmlKey = xmlSection.firstChildElement ( QString ( "%1_base64" ).arg ( to.name ) );
        }
        if ( xmlKey.isNull() )
        {
            // explicitly clear that the value was not found
            xmlText = QString();
            return true;
        }

        // Not converted from base64 yet if so stored
        xmlText = xmlKey.text();

        return true;
    }

    static inline bool writeXmlDefault ( TOption<T>& to, QDomDocument& xmlDoc, QDomElement& xmlSection, const QString& xmlText, QString& errmsg )
    {
        if ( ( to.behaviours & Option::Setting ) == 0 )
        {
            errmsg = QString ( "%1 is not a setting.  Please raise a bug report." ).arg ( to.name );
            return false;
        }

        QDomText    xmlNodeValue = xmlDoc.createTextNode ( xmlText );
        QDomElement xmlNode      = xmlDoc.createElement ( to.name );
        xmlNode.appendChild ( xmlNodeValue );
        xmlSection.appendChild ( xmlNode );

        return true;
    }
};

class StringOption : public TOption<QString>
{
    typedef bool ( *StringOptionValidator ) ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg );
    typedef bool ( *StringOptionXmlReader ) ( StringOption& ref, const QDomElement& xmlSection, QString& errmsg );
    typedef bool ( *StringOptionXmlWriter ) ( StringOption& ref, QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg );

public:
    StringOption ( const StringOption& so ) : TOption<QString> ( so ), validator ( so.validator ), xmlReader ( so.xmlReader ) {}
    StringOption ( QString               nameX,
                   Behaviours            behavioursX,
                   QList<QString>        shortOptionsX,
                   QList<QString>        longOptionsX,
                   QString               helpTextX,
                   QString               validTextX,
                   QString               defvalueX,
                   StringOptionValidator validatorX,
                   StringOptionXmlReader xmlReaderX ) :
        TOption<QString> ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, 1, defvalueX, defvalueX ),
        validator ( validatorX ),
        xmlReader ( xmlReaderX )
    {}
    StringOption ( QString               nameX,
                   Behaviours            behavioursX,
                   QList<QString>        shortOptionsX,
                   QList<QString>        longOptionsX,
                   QString               helpTextX,
                   QString               validTextX,
                   QString               defvalueX,
                   StringOptionValidator validatorX ) :
        TOption<QString> ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, 1, defvalueX, defvalueX ),
        validator ( validatorX )
    {}
    StringOption ( QString        nameX,
                   Behaviours     behavioursX,
                   QList<QString> shortOptionsX,
                   QList<QString> longOptionsX,
                   QString        helpTextX,
                   QString        validTextX,
                   QString        defvalueX ) :
        TOption<QString> ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, 1, defvalueX, defvalueX )
    {}
    StringOption ( QString nameX, Behaviours behavioursX, QList<QString> shortOptionsX, QList<QString> longOptionsX, QString helpTextX ) :
        TOption<QString> ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, QString(), 1, QString(), QString() )
    {}

    StringOption ( QString nameX, Behaviours behavioursX, StringOptionValidator validatorX, StringOptionXmlReader xmlReaderX ) :
        TOption<QString> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, QString(), QString() ),
        validator ( validatorX ),
        xmlReader ( xmlReaderX )
    {}
    StringOption ( QString nameX, Behaviours behavioursX, StringOptionValidator validatorX ) :
        TOption<QString> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, QString(), QString() ),
        validator ( validatorX )
    {}
    StringOption ( QString nameX, Behaviours behavioursX ) :
        TOption<QString> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, QString(), QString() )
    {}

    StringOption ( QString nameX, Behaviours behavioursX, QString defvalueX, StringOptionValidator validatorX, StringOptionXmlReader xmlReaderX ) :
        TOption<QString> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, defvalueX, defvalueX ),
        validator ( validatorX ),
        xmlReader ( xmlReaderX )
    {}
    StringOption ( QString nameX, Behaviours behavioursX, QString defvalueX, StringOptionValidator validatorX ) :
        TOption<QString> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, defvalueX, defvalueX ),
        validator ( validatorX )
    {}
    StringOption ( QString nameX, Behaviours behavioursX, QString defvalueX ) :
        TOption<QString> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, defvalueX, defvalueX )
    {}

    StringOptionValidator validator = allValid;

    StringOptionXmlReader xmlReader = [] ( StringOption& ref, const QDomElement& xmlSection, QString& errmsg ) {
        return ref.readXmlDefault ( xmlSection, errmsg, ref.validator );
    };

    StringOptionXmlWriter xmlWriter = [] ( StringOption& ref, QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg ) {
        QString xmlText = ( ref.behaviours & Option::IsBase64 ) ? ref.toBase64String() : ref.value;
        return ref.value == ref.defvalue || TOption<QString>::writeXmlDefault ( ref, xmlDoc, xmlSection, xmlText, errmsg );
    };

    static bool allValid ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg );

    static bool validFile ( StringOption&  ref,
                            QList<QString> args,
                            QString&       successmsg,
                            QString&       errmsg,
                            QString        errmsgWrapper,
                            bool           readOnly,
                            bool           mustExist,
                            bool           loadAsValue );

    bool validFile ( QString& errmsg, bool readOnly, bool mustExist, bool loadAsValue );

    static bool validDir ( StringOption& ref, QList<QString> args, QString& successmsg, QString& errmsg, QString errmsgWrapper, bool mustExist );

    bool validDir ( QString& errmsg, bool mustExist );

    StringOption& operator= ( StringOption rhs );

    inline QString toString() override { return value; }

    inline void    fromBase64String ( const QString& base64String ) { value = stringFromBase64 ( base64String ); }
    inline QString toBase64String() const { return QString::fromLatin1 ( value.toUtf8().toBase64() ); }

    bool readXmlDefault ( const QDomElement& xmlSection, QString& errmsg, StringOptionValidator validatorX );

    inline bool readXML ( const QDomElement& xmlSection, QString& errmsg ) override { return xmlReader ( *this, xmlSection, errmsg ); }

    inline bool writeXML ( QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg ) override
    {
        return xmlWriter ( *this, xmlDoc, xmlSection, errmsg );
    }
};

class FlagOption : public TOption<bool>
{
    typedef bool ( *FlagOptionValidator ) ( FlagOption& ref, QList<QString> args, QString& successmsg, QString& errmsg );
    typedef bool ( *FlagOptionXmlReader ) ( FlagOption& ref, const QDomElement& xmlSection, QString& errmsg );
    typedef bool ( *FlagOptionXmlWriter ) ( FlagOption& ref, QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg );

public:
    FlagOption ( const FlagOption& fo ) : TOption<bool> ( fo ), validator ( fo.validator ) {}
    FlagOption ( QString             nameX,
                 Behaviours          behavioursX,
                 QList<QString>      shortOptionsX,
                 QList<QString>      longOptionsX,
                 QString             helpTextX,
                 QString             validTextX,
                 int                 nArgsX,
                 FlagOptionValidator validatorX,
                 bool                defvalueX ) :
        TOption<bool> ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, nArgsX, defvalueX, defvalueX ),
        validator ( validatorX )
    {}
    FlagOption ( QString        nameX,
                 Behaviours     behavioursX,
                 QList<QString> shortOptionsX,
                 QList<QString> longOptionsX,
                 QString        helpTextX,
                 QString        validTextX ) :
        TOption<bool> ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, 0, false, false )
    {}
    FlagOption ( QString nameX, Behaviours behavioursX, QList<QString> shortOptionsX, QList<QString> longOptionsX, QString helpTextX ) :
        TOption<bool> ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, QString(), 0, false, false )
    {}
    FlagOption ( QString nameX, Behaviours behavioursX, FlagOptionValidator validatorX ) :
        TOption<bool> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, false, false ),
        validator ( validatorX )
    {}

    FlagOptionValidator validator = [] ( FlagOption& ref, QList<QString>, QString& successmsg, QString& ) {
        ref.value  = !ref.defvalue;
        successmsg = ref.validText.contains ( "%1" ) ? ref.validText.arg ( ref.value ) : ref.validText;
        return true;
    };

    FlagOptionXmlReader xmlReader = [] ( FlagOption& ref, const QDomElement& xmlSection, QString& errmsg ) {
        return ref.readXmlDefault ( xmlSection, errmsg, ref.validator );
    };

    FlagOptionXmlWriter xmlWriter = [] ( FlagOption& ref, QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg ) {
        QString xmlText = ref.toString();
        return ref.value == ref.defvalue || TOption<bool>::writeXmlDefault ( ref, xmlDoc, xmlSection, xmlText, errmsg );
    };

    static bool allValid ( FlagOption& ref, QList<QString> args, QString& successmsg, QString& errmsg );

    FlagOption& operator= ( FlagOption rhs );

    inline QString toString() override { return QString ( "%1" ).arg ( value ); }

    bool readXmlDefault ( const QDomElement& xmlSection, QString& errmsg, FlagOptionValidator validatorX );

    inline bool readXML ( const QDomElement& xmlSection, QString& errmsg ) override { return xmlReader ( *this, xmlSection, errmsg ); }

    inline bool writeXML ( QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg ) override
    {
        return xmlWriter ( *this, xmlDoc, xmlSection, errmsg );
    }
};

class IntOption : public TOption<qint64>
{
    typedef bool ( *IntOptionValidator ) ( IntOption& ref, QList<QString> args, QString& successmsg, QString& errmsg );
    typedef bool ( *IntOptionXmlReader ) ( IntOption& ref, const QDomElement& xmlSection, QString& errmsg );
    typedef bool ( *IntOptionXmlWriter ) ( IntOption& ref, QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg );

public:
    IntOption ( const IntOption& io ) :
        TOption<qint64> ( io ),
        minvalue ( io.minvalue ),
        maxvalue ( io.maxvalue ),
        validator ( io.validator ),
        xmlReader ( io.xmlReader )
    {}
    IntOption ( QString            nameX,
                Behaviours         behavioursX,
                QList<QString>     shortOptionsX,
                QList<QString>     longOptionsX,
                QString            helpTextX,
                QString            validTextX,
                qint64             defvalueX,
                qint64             minvalueX,
                qint64             maxvalueX,
                IntOptionValidator validatorX,
                IntOptionXmlReader xmlReaderX ) :
        TOption<qint64> ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, 1, defvalueX, defvalueX ),
        minvalue ( minvalueX ),
        maxvalue ( maxvalueX ),
        validator ( validatorX ),
        xmlReader ( xmlReaderX )
    {}
    IntOption ( QString            nameX,
                Behaviours         behavioursX,
                QList<QString>     shortOptionsX,
                QList<QString>     longOptionsX,
                QString            helpTextX,
                QString            validTextX,
                qint64             defvalueX,
                qint64             minvalueX,
                qint64             maxvalueX,
                IntOptionValidator validatorX ) :
        TOption<qint64> ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, 1, defvalueX, defvalueX ),
        minvalue ( minvalueX ),
        maxvalue ( maxvalueX ),
        validator ( validatorX )
    {}
    IntOption ( QString            nameX,
                Behaviours         behavioursX,
                QList<QString>     shortOptionsX,
                QList<QString>     longOptionsX,
                QString            helpTextX,
                QString            validTextX,
                qint64             defvalueX,
                qint64             minvalueX,
                qint64             maxvalueX,
                IntOptionXmlReader xmlReaderX ) :
        TOption<qint64> ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, 1, defvalueX, defvalueX ),
        minvalue ( minvalueX ),
        maxvalue ( maxvalueX ),
        xmlReader ( xmlReaderX )
    {}
    IntOption ( QString        nameX,
                Behaviours     behavioursX,
                QList<QString> shortOptionsX,
                QList<QString> longOptionsX,
                QString        helpTextX,
                QString        validTextX,
                qint64         defvalueX,
                qint64         minvalueX,
                qint64         maxvalueX ) :
        TOption<qint64> ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, 1, defvalueX, defvalueX ),
        minvalue ( minvalueX ),
        maxvalue ( maxvalueX )
    {}
    IntOption ( QString        nameX,
                Behaviours     behavioursX,
                QList<QString> shortOptionsX,
                QList<QString> longOptionsX,
                QString        helpTextX,
                QString        validTextX,
                qint64         defvalueX ) :
        TOption<qint64> ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, 1, defvalueX, defvalueX )
    {}
    IntOption ( QString nameX, Behaviours behavioursX, QList<QString> shortOptionsX, QList<QString> longOptionsX, QString helpTextX ) :
        TOption<qint64> ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, QString(), 1, 0, 0 )
    {}
    IntOption ( QString nameX, Behaviours behavioursX, qint64 defvalueX, qint64 minvalueX, qint64 maxvalueX ) :
        TOption<qint64> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, defvalueX, defvalueX ),
        minvalue ( minvalueX ),
        maxvalue ( maxvalueX )
    {}
    IntOption ( QString nameX, Behaviours behavioursX, IntOptionValidator validatorX, IntOptionXmlReader xmlReaderX ) :
        TOption<qint64> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, 0, 0 ),
        validator ( validatorX ),
        xmlReader ( xmlReaderX )
    {}
    IntOption ( QString nameX, Behaviours behavioursX, IntOptionValidator validatorX ) :
        TOption<qint64> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, 0, 0 ),
        validator ( validatorX )
    {}
    IntOption ( QString nameX, Behaviours behavioursX, IntOptionXmlReader xmlReaderX ) :
        TOption<qint64> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, 0, 0 ),
        xmlReader ( xmlReaderX )
    {}
    IntOption ( QString nameX, Behaviours behavioursX, qint64 defvalueX, qint64 minvalueX, qint64 maxvalueX, IntOptionValidator validatorX ) :
        TOption<qint64> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, defvalueX, defvalueX ),
        minvalue ( minvalueX ),
        maxvalue ( maxvalueX ),
        validator ( validatorX )
    {}
    IntOption ( QString nameX, Behaviours behavioursX, qint64 defvalueX, qint64 minvalueX, qint64 maxvalueX, IntOptionXmlReader xmlReaderX ) :
        TOption<qint64> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, defvalueX, defvalueX ),
        minvalue ( minvalueX ),
        maxvalue ( maxvalueX ),
        xmlReader ( xmlReaderX )
    {}

    qint64             minvalue  = 0;
    qint64             maxvalue  = Q_INT64_C ( 0x7fffffffffffffff );
    IntOptionValidator validator = allValid;

    IntOptionXmlReader xmlReader = [] ( IntOption& ref, const QDomElement& xmlSection, QString& errmsg ) {
        return ref.readXmlDefault ( xmlSection, errmsg, ref.validator );
    };

    IntOptionXmlWriter xmlWriter = [] ( IntOption& ref, QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg ) {
        QString xmlText = ref.toString();
        return ref.value == ref.defvalue || TOption<qint64>::writeXmlDefault ( ref, xmlDoc, xmlSection, xmlText, errmsg );
    };

    static bool allValid ( IntOption& ref, QList<QString> args, QString& successmsg, QString& errmsg );

    IntOption& operator= ( IntOption rhs );

    inline QString toString() override { return QString ( "%1" ).arg ( value ); }

    inline int toInt() { return static_cast<int> ( value ); }

    bool readXmlDefault ( const QDomElement& xmlSection, QString& errmsg, IntOptionValidator validatorXml );

    inline bool readXML ( const QDomElement& xmlSection, QString& errmsg ) override { return xmlReader ( *this, xmlSection, errmsg ); }

    inline bool writeXML ( QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg ) override
    {
        return xmlWriter ( *this, xmlDoc, xmlSection, errmsg );
    }
};

class ByteArrayOption : public TOption<QByteArray>
{
    typedef bool ( *ByteArrayOptionValidator ) ( ByteArrayOption& ref, QList<QString> args, QString& successmsg, QString& errmsg );
    typedef bool ( *ByteArrayOptionXmlReader ) ( ByteArrayOption& ref, const QDomElement& xmlSection, QString& errmsg );
    typedef bool ( *ByteArrayOptionXmlWriter ) ( ByteArrayOption& ref, QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg );

public:
    ByteArrayOption ( const ByteArrayOption& bo ) : TOption<QByteArray> ( bo ), validator ( bo.validator ), xmlReader ( bo.xmlReader ) {}
    ByteArrayOption ( QString                  nameX,
                      Behaviours               behavioursX,
                      QList<QString>           shortOptionsX,
                      QList<QString>           longOptionsX,
                      QString                  helpTextX,
                      QString                  validTextX,
                      int                      nArgsX,
                      QByteArray               defvalueX,
                      ByteArrayOptionValidator validatorX,
                      ByteArrayOptionXmlReader xmlReaderX ) :
        TOption<QByteArray> ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, nArgsX, defvalueX, defvalueX ),
        validator ( validatorX ),
        xmlReader ( xmlReaderX )
    {}
    ByteArrayOption ( QString                  nameX,
                      Behaviours               behavioursX,
                      QByteArray               defvalueX,
                      ByteArrayOptionValidator validatorX,
                      ByteArrayOptionXmlReader xmlReaderX ) :
        TOption<QByteArray> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, defvalueX, defvalueX ),
        validator ( validatorX ),
        xmlReader ( xmlReaderX )
    {}
    ByteArrayOption ( QString nameX, Behaviours behavioursX, QByteArray defvalueX ) :
        TOption<QByteArray> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, defvalueX, defvalueX ),
        validator ( allValid )
    {}
    ByteArrayOption ( QString nameX, Behaviours behavioursX ) :
        TOption<QByteArray> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, byteArrayFromBase64 ( "" ), byteArrayFromBase64 ( "" ) ),
        validator ( allValid )
    {}
    ByteArrayOption ( QString nameX, Behaviours behavioursX, ByteArrayOptionXmlReader xmlReaderX ) :
        TOption<QByteArray> ( nameX, behavioursX, {}, {}, QString(), QString(), 0, byteArrayFromBase64 ( "" ), byteArrayFromBase64 ( "" ) ),
        xmlReader ( xmlReaderX )
    {}

    ByteArrayOptionValidator validator = [] ( ByteArrayOption&, QList<QString>, QString&, QString& ) { return false; };

    ByteArrayOptionXmlReader xmlReader = [] ( ByteArrayOption& ref, const QDomElement& xmlSection, QString& errmsg ) {
        return ref.readXmlDefault ( xmlSection, errmsg, ref.validator );
    };

    ByteArrayOptionXmlWriter xmlWriter = [] ( ByteArrayOption& ref, QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg ) {
        QString xmlText = ref.toBase64String();
        return ref.value == ref.defvalue || TOption<QByteArray>::writeXmlDefault ( ref, xmlDoc, xmlSection, xmlText, errmsg );
    };

    static bool allValid ( ByteArrayOption& ref, QList<QString> args, QString& successmsg, QString& errmsg );

    ByteArrayOption& operator= ( ByteArrayOption rhs );

    inline void    fromBase64String ( const QString& base64String ) { value = byteArrayFromBase64 ( base64String ); }
    inline QString toBase64String() const { return QString::fromLatin1 ( value.toBase64() ); }

    bool readXmlDefault ( const QDomElement& xmlSection, QString& errmsg, ByteArrayOption::ByteArrayOptionValidator validatorX );

    inline bool readXML ( const QDomElement& xmlSection, QString& errmsg ) override { return xmlReader ( *this, xmlSection, errmsg ); }

    inline bool writeXML ( QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg ) override
    {
        return xmlWriter ( *this, xmlDoc, xmlSection, errmsg );
    }
};

template<typename T>
class VectorOption : public Option
{
    typedef bool ( *VectorOptionValidator ) ( VectorOption<T>& ref, QList<QString> args, QString& successmsg, QString& errmsg );
    typedef bool ( *VectorOptionXmlReader ) ( VectorOption<T>& ref, const QDomElement& xmlSection, QString& errmsg );
    typedef bool ( *VectorOptionXmlWriter ) ( VectorOption<T>& ref, QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg );

public:
    VectorOption<T> ( const VectorOption<T>& vo ) :
        Option ( vo.name, vo.behaviours, vo.shortOptions, vo.longOptions, vo.helpText, vo.validText, vo.nArgs ),
        minsize ( vo.minsize ),
        maxsize ( vo.maxsize ),
        defvalue ( vo.defvalue ),
        value ( vo.value ),
        validator ( vo.validator ),
        xmlReader ( vo.xmlReader )
    {}
    VectorOption<T> ( QString               nameX,
                      Behaviours            behavioursX,
                      QList<QString>        shortOptionsX,
                      QList<QString>        longOptionsX,
                      QString               helpTextX,
                      QString               validTextX,
                      int                   nArgsX,
                      size_t                minsizeX,
                      size_t                maxsizeX,
                      CVector<T>            defvalueX,
                      CVector<T>            valueX,
                      VectorOptionValidator validatorX,
                      VectorOptionXmlReader xmlReaderX ) :
        Option ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, nArgsX ),
        minsize ( minsizeX ),
        maxsize ( maxsizeX ),
        defvalue ( defvalueX ),
        value ( valueX ),
        validator ( validatorX ),
        xmlReader ( xmlReaderX )
    {}
    VectorOption<T> ( QString               nameX,
                      Behaviours            behavioursX,
                      QList<QString>        shortOptionsX,
                      QList<QString>        longOptionsX,
                      QString               helpTextX,
                      QString               validTextX,
                      int                   nArgsX,
                      size_t                minsizeX,
                      size_t                maxsizeX,
                      CVector<T>            defvalueX,
                      CVector<T>            valueX,
                      VectorOptionValidator validatorX ) :
        Option ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, nArgsX ),
        minsize ( minsizeX ),
        maxsize ( maxsizeX ),
        defvalue ( defvalueX ),
        value ( valueX ),
        validator ( validatorX )
    {}
    VectorOption<T> ( QString               nameX,
                      Behaviours            behavioursX,
                      QList<QString>        shortOptionsX,
                      QList<QString>        longOptionsX,
                      QString               helpTextX,
                      QString               validTextX,
                      int                   nArgsX,
                      size_t                minsizeX,
                      size_t                maxsizeX,
                      CVector<T>            defvalueX,
                      CVector<T>            valueX,
                      VectorOptionXmlReader xmlReaderX ) :
        Option ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, nArgsX ),
        minsize ( minsizeX ),
        maxsize ( maxsizeX ),
        defvalue ( defvalueX ),
        value ( valueX ),
        xmlReader ( xmlReaderX )
    {}
    VectorOption<T> ( QString               nameX,
                      Behaviours            behavioursX,
                      QList<QString>        shortOptionsX,
                      QList<QString>        longOptionsX,
                      QString               helpTextX,
                      QString               validTextX,
                      int                   nArgsX,
                      size_t                maxsizeX,
                      CVector<T>            defvalueX,
                      VectorOptionValidator validatorX,
                      VectorOptionXmlReader xmlReaderX ) :
        Option ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, nArgsX ),
        maxsize ( maxsizeX ),
        defvalue ( defvalueX ),
        value ( defvalueX ),
        validator ( validatorX ),
        xmlReader ( xmlReaderX )
    {}
    VectorOption<T> ( QString               nameX,
                      Behaviours            behavioursX,
                      QList<QString>        shortOptionsX,
                      QList<QString>        longOptionsX,
                      QString               helpTextX,
                      QString               validTextX,
                      int                   nArgsX,
                      size_t                maxsizeX,
                      CVector<T>            defvalueX,
                      VectorOptionValidator validatorX ) :
        Option ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, nArgsX ),
        maxsize ( maxsizeX ),
        defvalue ( defvalueX ),
        value ( defvalueX ),
        validator ( validatorX )
    {}
    VectorOption<T> ( QString               nameX,
                      Behaviours            behavioursX,
                      QList<QString>        shortOptionsX,
                      QList<QString>        longOptionsX,
                      QString               helpTextX,
                      QString               validTextX,
                      int                   nArgsX,
                      size_t                maxsizeX,
                      CVector<T>            defvalueX,
                      VectorOptionXmlReader xmlReaderX ) :
        Option ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, nArgsX ),
        maxsize ( maxsizeX ),
        defvalue ( defvalueX ),
        value ( defvalueX ),
        xmlReader ( xmlReaderX )
    {}
    VectorOption<T> ( QString               nameX,
                      Behaviours            behavioursX,
                      QList<QString>        shortOptionsX,
                      QList<QString>        longOptionsX,
                      QString               helpTextX,
                      QString               validTextX,
                      int                   nArgsX,
                      size_t                maxsizeX,
                      VectorOptionValidator validatorX,
                      VectorOptionXmlReader xmlReaderX ) :
        Option ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, nArgsX ),
        maxsize ( maxsizeX ),
        validator ( validatorX ),
        xmlReader ( xmlReaderX )
    {}
    VectorOption<T> ( QString               nameX,
                      Behaviours            behavioursX,
                      QList<QString>        shortOptionsX,
                      QList<QString>        longOptionsX,
                      QString               helpTextX,
                      QString               validTextX,
                      int                   nArgsX,
                      size_t                maxsizeX,
                      VectorOptionValidator validatorX ) :
        Option ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, nArgsX ),
        maxsize ( maxsizeX ),
        validator ( validatorX )
    {}
    VectorOption<T> ( QString               nameX,
                      Behaviours            behavioursX,
                      QList<QString>        shortOptionsX,
                      QList<QString>        longOptionsX,
                      QString               helpTextX,
                      QString               validTextX,
                      int                   nArgsX,
                      size_t                maxsizeX,
                      VectorOptionXmlReader xmlReaderX ) :
        Option ( nameX, behavioursX, shortOptionsX, longOptionsX, helpTextX, validTextX, nArgsX ),
        maxsize ( maxsizeX ),
        xmlReader ( xmlReaderX )
    {}
    VectorOption<T> ( QString               nameX,
                      Behaviours            behavioursX,
                      size_t                maxsizeX,
                      CVector<T>            defvalueX,
                      VectorOptionValidator validatorX,
                      VectorOptionXmlReader xmlReaderX ) :
        Option ( nameX, behavioursX, {}, {}, QString(), QString(), 0 ),
        maxsize ( maxsizeX ),
        defvalue ( defvalueX ),
        value ( defvalueX ),
        validator ( validatorX ),
        xmlReader ( xmlReaderX )
    {}
    VectorOption<T> ( QString nameX, Behaviours behavioursX, size_t maxsizeX, CVector<T> defvalueX, VectorOptionValidator validatorX ) :
        Option ( nameX, behavioursX, {}, {}, QString(), QString(), 0 ),
        maxsize ( maxsizeX ),
        defvalue ( defvalueX ),
        value ( defvalueX ),
        validator ( validatorX )
    {}
    VectorOption<T> ( QString nameX, Behaviours behavioursX, size_t maxsizeX, CVector<T> defvalueX, VectorOptionXmlReader xmlReaderX ) :
        Option ( nameX, behavioursX, {}, {}, QString(), QString(), 0 ),
        maxsize ( maxsizeX ),
        defvalue ( defvalueX ),
        value ( defvalueX ),
        xmlReader ( xmlReaderX )
    {}
    VectorOption<T> ( QString nameX, Behaviours behavioursX, size_t maxsizeX, VectorOptionValidator validatorX, VectorOptionXmlReader xmlReaderX ) :
        Option ( nameX, behavioursX, {}, {}, QString(), QString(), 0 ),
        maxsize ( maxsizeX ),
        validator ( validatorX ),
        xmlReader ( xmlReaderX )
    {}
    VectorOption<T> ( QString nameX, Behaviours behavioursX, size_t maxsizeX, VectorOptionValidator validatorX ) :
        Option ( nameX, behavioursX, {}, {}, QString(), QString(), 0 ),
        maxsize ( maxsizeX ),
        validator ( validatorX )
    {}
    VectorOption<T> ( QString nameX, Behaviours behavioursX, size_t maxsizeX, VectorOptionXmlReader xmlReaderX ) :
        Option ( nameX, behavioursX, {}, {}, QString(), QString(), 0 ),
        maxsize ( maxsizeX ),
        xmlReader ( xmlReaderX )
    {}

    VectorOption<T> ( QString               nameX,
                      Behaviours            behavioursX,
                      size_t                maxsizeX,
                      T                     defvalueX,
                      VectorOptionValidator validatorX,
                      VectorOptionXmlReader xmlReaderX ) :
        Option ( nameX, behavioursX, {}, {}, QString(), QString(), 0 ),
        maxsize ( maxsizeX ),
        validator ( validatorX ),
        xmlReader ( xmlReaderX )
    {
        for ( size_t i = 0; i < maxsize; i++ )
        {
            defvalue[i] = defvalueX;
            value[i]    = defvalue[i];
        }
    }
    VectorOption<T> ( QString nameX, Behaviours behavioursX, size_t maxsizeX, T defvalueX, VectorOptionValidator validatorX ) :
        Option ( nameX, behavioursX, {}, {}, QString(), QString(), 0 ),
        maxsize ( maxsizeX ),
        validator ( validatorX )
    {
        for ( size_t i = 0; i < maxsize; i++ )
        {
            defvalue[i] = defvalueX;
            value[i]    = defvalue[i];
        }
    }
    VectorOption<T> ( QString nameX, Behaviours behavioursX, size_t maxsizeX, T defvalueX, VectorOptionXmlReader xmlReaderX ) :
        Option ( nameX, behavioursX, {}, {}, QString(), QString(), 0 ),
        maxsize ( maxsizeX ),
        xmlReader ( xmlReaderX )
    {
        for ( size_t i = 0; i < maxsize; i++ )
        {
            defvalue[i] = defvalueX;
            value[i]    = defvalue[i];
        }
    }

    ~VectorOption<T>() override {}

    size_t     minsize  = 0;
    size_t     maxsize  = 256;
    CVector<T> defvalue = CVector<T> ( static_cast<int> ( maxsize ) );
    CVector<T> value    = CVector<T> ( static_cast<int> ( maxsize ) );

    VectorOptionValidator validator = [] ( VectorOption<T>&, QList<QString>, QString&, QString& errmsg ) {
        errmsg = "Nothing is valid";
        return false;
    };

    VectorOptionXmlReader xmlReader = [] ( VectorOption<T>& ref, const QDomElement& xmlSection, QString& errmsg ) {
        return ref.readXmlDefault ( xmlSection, errmsg, ref.validator );
    };

    VectorOptionXmlWriter xmlWriter = [] ( VectorOption<T>& ref, QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg ) {
        QString xmlText = ref.toXmlText();
        return ref.value == ref.defvalue || VectorOption<T>::writeXmlDefault ( ref, xmlDoc, xmlSection, xmlText, errmsg );
    };

    inline bool readXmlDefault ( const QDomElement& xmlSection, QString& errmsg, VectorOptionValidator validatorXml )
    {
        QString          xmlText;
        TOption<QString> so = TOption<QString> ( name, behaviours, {}, {}, QString(), QString(), 0, QString(), QString() );

        // Get a string
        if ( !TOption<QString>::readXmlDefault ( so, xmlSection, xmlText, errmsg ) )
        {
            return false;
        }
        if ( xmlText == QString() )
        {
            return true;
        }

        QString msg;
        return validatorXml ( *this, { xmlText }, msg, errmsg );
    }

    // xmlText is the CVector<T> as a string already
    static inline bool writeXmlDefault ( VectorOption<T>& to, QDomDocument& xmlDoc, QDomElement& xmlSection, const QString& xmlText, QString& errmsg )
    {
        TOption<QString> so = TOption<QString> ( to.name, to.behaviours, {}, {}, QString(), QString(), 0, QString(), QString() );
        return TOption<QString>::writeXmlDefault ( so, xmlDoc, xmlSection, xmlText, errmsg );
    }

    VectorOption<T>& operator= ( VectorOption<T> rhs )
    {
        Option::operator= ( rhs );
        validator = rhs.validator;
        minsize   = rhs.minsize;
        maxsize   = rhs.maxsize;
        defvalue  = rhs.defvalue;
        value     = rhs.value;
        return *this;
    }

    virtual bool isEmpty()
    {
        for ( size_t i = 0; i < maxsize; i++ )
        {
            if ( value.at ( i ) != defvalue.at ( i ) )
            {
                return false;
            }
        }
        return true;
    }

    bool readXML ( const QDomElement& xmlSection, QString& errmsg ) override { return xmlReader ( *this, xmlSection, errmsg ); }

    bool writeXML ( QDomDocument& xmlDoc, QDomElement& xmlSection, QString& errmsg ) override
    {
        return xmlWriter ( *this, xmlDoc, xmlSection, errmsg );
    }

    virtual QString toXmlText() { return QString(); }
};

class StringVecOption : public VectorOption<QString>
{
    typedef bool ( *StringVecOptionValidator ) ( VectorOption<QString>& ref, QList<QString> args, QString& successmsg, QString& errmsg );
    typedef bool ( *StringVecOptionXmlReader ) ( VectorOption<QString>& ref, const QDomElement& xmlSection, QString& errmsg );

public:
    StringVecOption ( const VectorOption<QString>& vo ) : VectorOption<QString> ( vo ) {}
    StringVecOption ( QString                  nameX,
                      Behaviours               behavioursX,
                      QList<QString>           shortOptionsX,
                      QList<QString>           longOptionsX,
                      QString                  helpTextX,
                      QString                  validTextX,
                      int                      nArgsX,
                      size_t                   minsizeX,
                      size_t                   maxsizeX,
                      CVector<QString>         defvalueX,
                      CVector<QString>         valueX,
                      StringVecOptionValidator validatorX,
                      StringVecOptionXmlReader xmlReaderX ) :
        VectorOption<QString> ( nameX,
                                behavioursX,
                                shortOptionsX,
                                longOptionsX,
                                helpTextX,
                                validTextX,
                                nArgsX,
                                minsizeX,
                                maxsizeX,
                                defvalueX,
                                valueX,
                                validatorX,
                                xmlReaderX )
    {}
    StringVecOption ( QString                  nameX,
                      Behaviours               behavioursX,
                      size_t                   maxsizeX,
                      QString                  defvalueX,
                      StringVecOptionValidator validatorX,
                      StringVecOptionXmlReader xmlReaderX ) :
        VectorOption<QString> ( nameX, behavioursX, maxsizeX, defvalueX, validatorX, xmlReaderX )
    {}
    StringVecOption ( QString                  nameX,
                      Behaviours               behavioursX,
                      size_t                   maxsizeX,
                      StringVecOptionValidator validatorX,
                      StringVecOptionXmlReader xmlReaderX ) :
        VectorOption<QString> ( nameX, behavioursX, maxsizeX, validatorX, xmlReaderX )
    {}

    static bool allValid ( VectorOption<QString>& ref, QList<QString> args, QString& successmsg, QString& errmsg );

    QString toString() override;

    QString toXmlText() override;
};

class FlagVecOption : public VectorOption<bool>
{
    typedef bool ( *FlagVecOptionValidator ) ( VectorOption<bool>& ref, QList<QString> args, QString& successmsg, QString& errmsg );
    typedef bool ( *FlagVecOptionXmlReader ) ( VectorOption<bool>& ref, const QDomElement& xmlSection, QString& errmsg );

public:
    FlagVecOption ( const VectorOption<bool>& vo ) : VectorOption<bool> ( vo ) {}
    FlagVecOption ( QString                nameX,
                    Behaviours             behavioursX,
                    size_t                 maxsizeX,
                    bool                   defvalueX,
                    FlagVecOptionValidator validatorX,
                    FlagVecOptionXmlReader xmlReaderX ) :
        VectorOption<bool> ( nameX, behavioursX, maxsizeX, defvalueX, validatorX, xmlReaderX )
    {}
    FlagVecOption ( QString nameX, Behaviours behavioursX, size_t maxsizeX, FlagVecOptionValidator validatorX, FlagVecOptionXmlReader xmlReaderX ) :
        VectorOption<bool> ( nameX, behavioursX, maxsizeX, validatorX, xmlReaderX )
    {}

    static bool allValid ( VectorOption<bool>& ref, QList<QString> args, QString& successmsg, QString& errmsg );

    QString toString() override;

    QString toXmlText() override;
};

class IntVecOption : public VectorOption<int>
{
    typedef bool ( *IntVecOptionValidator ) ( VectorOption<int>& ref, QList<QString> args, QString& successmsg, QString& errmsg );
    typedef bool ( *IntVecOptionXmlReader ) ( VectorOption<int>& ref, const QDomElement& xmlSection, QString& errmsg );

public:
    IntVecOption ( const IntVecOption& vo ) : VectorOption<int> ( vo ) {}
    IntVecOption ( QString               nameX,
                   Behaviours            behavioursX,
                   size_t                maxsizeX,
                   int                   defvalueX,
                   IntVecOptionValidator validatorX,
                   IntVecOptionXmlReader xmlReaderX ) :
        VectorOption<int> ( nameX, behavioursX, maxsizeX, defvalueX, validatorX, xmlReaderX )
    {}
    IntVecOption ( QString nameX, Behaviours behavioursX, size_t maxsizeX, IntVecOptionValidator validatorX, IntVecOptionXmlReader xmlReaderX ) :
        VectorOption<int> ( nameX, behavioursX, maxsizeX, validatorX, xmlReaderX )
    {}

    static bool allValid ( VectorOption<int>& ref, QList<QString> args, QString& successmsg, QString& errmsg );

    QString toString() override;

    QString toXmlText() override;
};

// All Options used should live here.
struct JamulusOptions
{
    JamulusOptions();

    FlagOption      fo_help;
    FlagOption      fo_helphtml;
    FlagOption      fo_version;
    StringOption    so_inifile;
    FlagOption      fo_nogui;
    FlagOption      fo_quiet;
    FlagOption      fo_ipv6;
    IntOption       io_jsonrpcport;
    StringOption    so_jsonrpcsecret;
    StringOption    so_jsonrpcbindip;
    StringOption    so_language;
    FlagOption      fo_notranslation;
    IntOption       io_port;
    IntOption       io_qos;
    FlagOption      fo_discononquit;
    StringOption    so_directory;
    StringOption    so_directoryDeprecated;
    IntOption       io_directorytype;
    StringOption    so_serverlistfile;
    StringVecOption vs_serverlistfilter;
    FlagOption      fo_fastupdate;
    StringOption    so_logfile;
    FlagOption      fo_licence;
    StringOption    so_htmlstatus;
    StringOption    so_serverinfo;
    StringOption    so_serverpublicip;
    FlagOption      fo_delaypan;
    StringOption    so_recording;
    FlagOption      fo_norecord;
    FlagOption      fo_server;
    StringOption    so_serverbindip;
    FlagOption      fo_multithreading;
    IntOption       io_numchannels;
    StringOption    so_welcomemessage;
    FlagOption      fo_startminimized;
    StringOption    so_connect;
    FlagOption      fo_nojackconnect;
    FlagOption      fo_mutestream;
    FlagOption      fo_mutemyown;
    StringOption    so_clientname;
    StringOption    so_ctrlmidich;
    FlagOption      fo_audioalerts;
    FlagOption      fo_showallservers;
    FlagOption      fo_showanalyzerconsole;
    StringVecOption vo_serveraddresses;
    IntOption       io_newclientlevel;
    IntOption       io_inputboost;
    FlagOption      fo_detectfeedback;
    FlagOption      fo_connectdlgshowallmusicians;
    IntOption       io_channelsort;
    FlagOption      fo_ownfaderfirst;
    IntOption       io_numrowsmixpan;
    StringOption    so_name;
    IntOption       io_instrument;
    IntOption       io_country;
    StringOption    so_city;
    IntOption       io_skill;
    IntOption       io_audfad;
    IntOption       io_revlev;
    FlagOption      fo_reverblchan;
    StringOption    so_auddev;
    IntOption       io_sndcrdinlch;
    IntOption       io_sndcrdinrch;
    IntOption       io_sndcrdoutlch;
    IntOption       io_sndcrdoutrch;
    IntOption       io_prefsndcrdbufidx;
    IntOption       io_audiochannels;
    IntOption       io_audioquality;
    FlagOption      fo_enableopussmall;
    FlagOption      fo_autojitbuf;
    IntOption       io_jitbuf;
    IntOption       io_jitbufserver;
    IntOption       io_guidesign;
    IntOption       io_meterstyle;
    IntOption       io_customdirectoryindex;
    StringVecOption vo_directoryaddresses;
    ByteArrayOption bo_winposmain;
    ByteArrayOption bo_winposset;
    ByteArrayOption bo_winposchat;
    ByteArrayOption bo_winposcon;
    FlagOption      fo_winvisset;
    FlagOption      fo_winvischat;
    FlagOption      fo_winviscon;
    IntOption       io_settingstab;
    StringVecOption vo_storedfadertag;
    IntVecOption    vo_storedfaderlevel;
    IntVecOption    vo_storedpanvalue;
    FlagVecOption   vo_storedfaderissolo;
    FlagVecOption   vo_storedfaderismute;
    IntVecOption    vo_storedgroupid;
};

// AllOptions allOptions holds actual instances of any Option.
extern struct JamulusOptions AllOptions;

// AnyOption is an extended union, with extra behaviour.
// I have not been able to get it to handle VectorOption<T>, unfortunately, hence StringVecOption and IntVecOption.
class AnyOption;
typedef QMap<QString, AnyOption*> AnyOptionsMap;
typedef QList<AnyOption>          AnyOptionList;

extern AnyOptionList settings;
extern AnyOptionList faderSettings;

class AnyOption
{
public:
    enum AOType
    {
        OTUnassigned = 0x00,
        OTString     = 0x01,
        OTFlag       = 0x02,
        OTInt        = 0x03,
        OTByteArray  = 0x04,
        OTStringVec  = 0x05,
        OTFlagVec    = 0x06,
        OTIntVec     = 0x07,
    };

private:
    union _oPtr
    {
        Option*          oPtr;
        StringOption*    soPtr;
        FlagOption*      foPtr;
        IntOption*       ioPtr;
        ByteArrayOption* boPtr;
        StringVecOption* svPtr;
        FlagVecOption*   fvPtr;
        IntVecOption*    ivPtr;
    };

    AOType aoType{ OTUnassigned };
    _oPtr  aoPtr;

    static bool checkServerOptions ( QList<AnyOption*>&, QString& errmsg );
#ifndef SERVER_ONLY
    static bool checkClientOptions ( QList<AnyOption*>& parsedOptions, QString& errmsg );
#endif
    static bool loadSettings ( bool server, bool nogui, QList<AnyOption*>& parsedOptions, bool quiet, QString& errmsg );
    static bool getXMLDocument ( const QString strFileName, QDomDocument& domXMLDoc, QString& errmsg );
    static bool readSettings ( AnyOptionList& options, const QDomDocument& domXMLDoc, QString& errmsg );

    static bool saveSettings();
    static bool putXMLDocument ( const QString strFileName, const QDomDocument& domXMLDoc, QString& errmsg );
    static bool writeSettings ( AnyOptionList& options, QDomDocument& XMLDocument, QDomElement& domXMLDoc, QString& errmsg );

    static QString getFormattedtSectionsHeadings ( QString format );

    static QString getOptionsAs ( AnyOptionList& options, Option::Behaviours behaviours, QString str1, QString str2 );

    static QString getOptionsAsHelpText ( AnyOptionList& options, Option::Behaviours behaviours );
    static QString getOptionsAsHelpText ( AnyOptionList& options );

    static QString getOptionsAsHTML ( AnyOptionList& options, Option::Behaviours behaviours );
    static QString getOptionsAsHTML ( AnyOptionList& options );

public:
    // Copy constructor
    AnyOption ( const AnyOption& other ) : aoType ( other.aoType ), aoPtr ( other.aoPtr ) {}

    // Default constructor - null
    AnyOption() { aoPtr.oPtr = nullptr; }

    // Primary wrapper constructors - aoPtr union sets all union members, including "untyped" oPtr
    AnyOption ( StringOption& other ) : aoType ( OTString ) { aoPtr.soPtr = &other; }
    AnyOption ( StringOption* other ) : aoType ( OTString ) { aoPtr.soPtr = other; }
    AnyOption ( FlagOption& other ) : aoType ( OTFlag ) { aoPtr.foPtr = &other; }
    AnyOption ( FlagOption* other ) : aoType ( OTFlag ) { aoPtr.foPtr = other; }
    AnyOption ( IntOption& other ) : aoType ( OTInt ) { aoPtr.ioPtr = &other; }
    AnyOption ( IntOption* other ) : aoType ( OTInt ) { aoPtr.ioPtr = other; }
    AnyOption ( ByteArrayOption& other ) : aoType ( OTInt ) { aoPtr.boPtr = &other; }
    AnyOption ( ByteArrayOption* other ) : aoType ( OTInt ) { aoPtr.boPtr = other; }
    AnyOption ( StringVecOption& other ) : aoType ( OTStringVec ) { aoPtr.svPtr = &other; }
    AnyOption ( StringVecOption* other ) : aoType ( OTStringVec ) { aoPtr.svPtr = other; }
    AnyOption ( FlagVecOption& other ) : aoType ( OTStringVec ) { aoPtr.fvPtr = &other; }
    AnyOption ( FlagVecOption* other ) : aoType ( OTStringVec ) { aoPtr.fvPtr = other; }
    AnyOption ( IntVecOption& other ) : aoType ( OTIntVec ) { aoPtr.ivPtr = &other; }
    AnyOption ( IntVecOption* other ) : aoType ( OTIntVec ) { aoPtr.ivPtr = other; }

    // "Dummy" wrapper constructors
    AnyOption ( Option& other ) : aoType ( OTUnassigned ) { aoPtr.oPtr = &other; }
    AnyOption ( Option* other ) : aoType ( OTUnassigned ) { aoPtr.oPtr = other; }

    // Standard getters
    inline AOType optionType() { return aoType; }

    inline Option*          optionPtr() { return aoPtr.oPtr; }
    inline StringOption*    stringOptionPtr() { return ( aoType == OTString ) ? aoPtr.soPtr : nullptr; }
    inline FlagOption*      flagOptionPtr() { return ( aoType == OTFlag ) ? aoPtr.foPtr : nullptr; }
    inline IntOption*       intOptionPtr() { return ( aoType == OTInt ) ? aoPtr.ioPtr : nullptr; }
    inline ByteArrayOption* byteArrayOptionPtr() { return ( aoType == OTByteArray ) ? aoPtr.boPtr : nullptr; }
    inline StringVecOption* stringVecOptionPtr() { return ( aoType == OTStringVec ) ? aoPtr.svPtr : nullptr; }
    inline FlagVecOption*   flagVecOptionPtr() { return ( aoType == OTFlagVec ) ? aoPtr.fvPtr : nullptr; }
    inline IntVecOption*    intVecOptionPtr() { return ( aoType == OTIntVec ) ? aoPtr.ivPtr : nullptr; }

    // Added functionality static getters
    static inline Option* option ( const AnyOptionsMap& aoMap, QString key ) { return aoMap[key] == nullptr ? nullptr : aoMap[key]->optionPtr(); }
    static inline StringOption* stringOption ( const AnyOptionsMap& aoMap, QString key )
    {
        return aoMap[key] == nullptr ? nullptr : aoMap[key]->stringOptionPtr();
    }
    static inline FlagOption* flagOption ( const AnyOptionsMap& aoMap, QString key )
    {
        return aoMap[key] == nullptr ? nullptr : aoMap[key]->flagOptionPtr();
    }
    static inline IntOption* intOption ( const AnyOptionsMap& aoMap, QString key )
    {
        return aoMap[key] == nullptr ? nullptr : aoMap[key]->intOptionPtr();
    }
    static inline ByteArrayOption* byteArrayOption ( const AnyOptionsMap& aoMap, QString key )
    {
        return aoMap[key] == nullptr ? nullptr : aoMap[key]->byteArrayOptionPtr();
    }
    static inline StringVecOption* stringVecOption ( const AnyOptionsMap& aoMap, QString key )
    {
        return aoMap[key] == nullptr ? nullptr : aoMap[key]->stringVecOptionPtr();
    }
    static inline FlagVecOption* flagVecOption ( const AnyOptionsMap& aoMap, QString key )
    {
        return aoMap[key] == nullptr ? nullptr : aoMap[key]->flagVecOptionPtr();
    }
    static inline IntVecOption* intVecOption ( const AnyOptionsMap& aoMap, QString key )
    {
        return aoMap[key] == nullptr ? nullptr : aoMap[key]->intVecOptionPtr();
    }

    static inline QString stringOptionValue ( const AnyOptionsMap& aoMap, QString key, QString defvalue = QString() )
    {
        return aoMap[key] != nullptr && aoMap[key]->aoPtr.oPtr->name == key && aoMap[key]->aoType == OTString ? aoMap[key]->aoPtr.soPtr->value
                                                                                                              : defvalue;
    }
    static inline bool flagOptionValue ( const AnyOptionsMap& aoMap, QString key, bool defValue = false )
    {
        return aoMap[key] != nullptr && aoMap[key]->aoPtr.oPtr->name == key && aoMap[key]->aoType == OTFlag ? aoMap[key]->aoPtr.foPtr->value
                                                                                                            : defValue;
    }
    static inline qint64 intOptionValue ( const AnyOptionsMap& aoMap, QString key, qint64 defValue = 0 )
    {
        return aoMap[key] != nullptr && aoMap[key]->aoPtr.oPtr->name == key && aoMap[key]->aoType == OTInt ? aoMap[key]->aoPtr.ioPtr->value
                                                                                                           : defValue;
    }
    static inline QByteArray byteArrayOptionValue ( const AnyOptionsMap& aoMap, QString key, QByteArray defValue = QByteArray() )
    {
        return aoMap[key] != nullptr && aoMap[key]->aoPtr.oPtr->name == key && aoMap[key]->aoType == OTByteArray ? aoMap[key]->aoPtr.boPtr->value
                                                                                                                 : defValue;
    }
    static inline CVector<QString> stringVecOptionValue ( const AnyOptionsMap& aoMap, QString key, CVector<QString> defvalue = CVector<QString>() )
    {
        return aoMap[key] != nullptr && aoMap[key]->aoPtr.oPtr->name == key && aoMap[key]->aoType == OTStringVec ? aoMap[key]->aoPtr.svPtr->value
                                                                                                                 : defvalue;
    }
    static inline CVector<bool> flagVecOptionValue ( const AnyOptionsMap& aoMap, QString key, CVector<bool> defvalue = CVector<bool>() )
    {
        return aoMap[key] != nullptr && aoMap[key]->aoPtr.oPtr->name == key && aoMap[key]->aoType == OTStringVec ? aoMap[key]->aoPtr.fvPtr->value
                                                                                                                 : defvalue;
    }
    static inline CVector<int> intVecOptionValue ( const AnyOptionsMap& aoMap, QString key, CVector<int> defValue = CVector<int>() )
    {
        return aoMap[key] != nullptr && aoMap[key]->aoPtr.oPtr->name == key && aoMap[key]->aoType == OTInt ? aoMap[key]->aoPtr.ivPtr->value
                                                                                                           : defValue;
    }

    // Added functionality static aggregate methods
    static bool ProcessArgs ( int argc, char** argv, QString& errmsg );
    static int  getAnyOption ( AnyOptionList& options, const QString& optionArg );
    static bool parse ( QStringList&       optionArgs,
                        AnyOptionList&     options,
                        QList<AnyOption*>& parsedOptions,
                        Option::Behaviours behaviours,
                        bool               quiet,
                        QString&           errmsg );
    static int  parsedOptionsIndexOf ( QList<AnyOption*>& list, Option& option );
    static bool parsedOptionsContains ( QList<AnyOption*>& list, Option& option );

    static QString usageArguments ( QString appName, AnyOptionList& options );
    static QString usageArgumentsHTML ( AnyOptionList& options );

    static bool SaveSettings() { return saveSettings(); }

    static bool LoadFaderSettings ( const QString strFileName );
    static bool SaveFaderSettings ( const QString strFileName );
};
