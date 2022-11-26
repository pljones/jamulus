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

#include <QObject>
#include <QDateTime>
#include <QHostAddress>
#include <QFileInfo>
#include <algorithm>
#ifdef USE_OPUS_SHARED_LIB
#    include "opus/opus_custom.h"
#else
#    include "opus_custom.h"
#endif
#include "global.h"
#include "buffer.h"
#include "signalhandler.h"
#include "socket.h"
#include "channel.h"
#include "util.h"
#include "serverlogging.h"
#include "serverlist.h"
#include "recorder/jamcontroller.h"

#include "threadpool.h"
#include "options.h"

/* Definitions ****************************************************************/
// no valid channel number
#define INVALID_CHANNEL_ID ( MAX_NUM_CHANNELS + 1 )

/* Classes ********************************************************************/
template<unsigned int slotId>
class CServerSlots : public CServerSlots<slotId - 1>
{
public:
    void OnSendProtMessCh ( CVector<uint8_t> mess ) { SendProtMessage ( slotId - 1, mess ); }
    void OnReqConnClientsListCh() { CreateAndSendChanListForThisChan ( slotId - 1 ); }

    void OnChatTextReceivedCh ( QString strChatText ) { CreateAndSendChatTextForAllConChannels ( slotId - 1, strChatText ); }

    void OnMuteStateHasChangedCh ( int iChanID, bool bIsMuted ) { CreateOtherMuteStateChanged ( slotId - 1, iChanID, bIsMuted ); }

    void OnServerAutoSockBufSizeChangeCh ( int iNNumFra ) { CreateAndSendJitBufMessage ( slotId - 1, iNNumFra ); }

protected:
    virtual void SendProtMessage ( int iChID, CVector<uint8_t> vecMessage ) = 0;

    virtual void CreateAndSendChanListForThisChan ( const int iCurChanID ) = 0;

    virtual void CreateAndSendChatTextForAllConChannels ( const int iCurChanID, const QString& strChatText ) = 0;

    virtual void CreateOtherMuteStateChanged ( const int iCurChanID, const int iOtherChanID, const bool bIsMuted ) = 0;

    virtual void CreateAndSendJitBufMessage ( const int iCurChanID, const int iNNumFra ) = 0;
};

template<>
class CServerSlots<0>
{};

class CServer : public QObject, public CServerSlots<MAX_NUM_CHANNELS>
{
    Q_OBJECT

public:
    CServer();

    virtual ~CServer();

    void Start();
    void Stop();
    bool IsRunning() { return HighPrecisionTimer.isActive(); }

    bool PutAudioData ( const CVector<uint8_t>& vecbyRecBuf, const int iNumBytesRead, const CHostAddress& HostAdr, int& iCurChanID );

    int GetNumberOfConnectedClients();

    void GetConCliParam ( CVector<CHostAddress>&     vecHostAddresses,
                          CVector<QString>&          vecsName,
                          CVector<int>&              veciJitBufNumFrames,
                          CVector<int>&              veciNetwFrameSizeFact,
                          CVector<CChannelCoreInfo>& vecChanInfo );

    void CreateCLServerListReqVerAndOSMes ( const CHostAddress& InetAddr ) { ConnLessProtocol.CreateCLReqVersionAndOSMes ( InetAddr ); }

    // GUI settings ------------------------------------------------------------
    inline QByteArray GetMainWindowGeometry() const { return AllOptions.bo_winposmain.value; }
    void              SetMainWindowGeometry ( const QByteArray value )
    {
        if ( AllOptions.bo_winposmain.value != value )
        {
            AllOptions.bo_winposmain.value = value;
        }
    }

    int GetClientNumAudioChannels ( const int iChanNum ) { return vecChannels[iChanNum].GetNumAudioChannels(); }

    inline QString GetDirectoryAddress() const { return ServerListManager.GetDirectoryAddress(); }
    inline void    SetDirectoryAddress ( const QString& sNDirectoryAddress ) { ServerListManager.SetDirectoryAddress ( sNDirectoryAddress ); }

    inline EDirectoryType GetDirectoryType() const { return ServerListManager.GetDirectoryType(); }
    inline void           SetDirectoryType ( const EDirectoryType eNCSAT ) { ServerListManager.SetDirectoryType ( eNCSAT ); }

    inline QString GetServerListFileName() const { return ServerListManager.GetServerListFileName(); }
    inline bool    SetServerListFileName ( const QString strFilename ) { return ServerListManager.SetServerListFileName ( strFilename ); }

    bool                 IsADirectory() const { return ServerListManager.IsADirectory(); }
    inline ESvrRegStatus GetSvrRegStatus() const { return ServerListManager.GetSvrRegStatus(); }

    // TODO: make the command line parser put these into separate allOption values - i.e. the same place the inifile reader puts them
    inline void             SetServerName ( const QString& strNewName ) { ServerListManager.SetServerName ( strNewName ); }
    inline QString          GetServerName() const { return ServerListManager.GetServerName(); }
    inline void             SetServerCity ( const QString& strNewCity ) { ServerListManager.SetServerCity ( strNewCity ); }
    inline QString          GetServerCity() const { return ServerListManager.GetServerCity(); }
    inline void             SetServerCountry ( const QLocale::Country eNewCountry ) { ServerListManager.SetServerCountry ( eNewCountry ); }
    inline QLocale::Country GetServerCountry() const { return ServerListManager.GetServerCountry(); }

    void           SetWelcomeMessage ( const QString& strNWelcMess );
    inline QString GetWelcomeMessage() const { return strWelcomeMessage; }

    inline QString GetLanguage() const { return AllOptions.so_language.value; }
    void           SetLanguage ( const QString value )
    {
        if ( AllOptions.so_language.value != value )
        {
            AllOptions.so_language.value = value;
            emit LanguageChanged ( AllOptions.so_language.value, AllOptions.fo_notranslation.value );
        }
    }

    inline bool IsAutoRunMinimized() const { return AllOptions.fo_startminimized.value; }
    void        SetAutoRunMinimized ( const bool NAuRuMin )
    {
        if ( AllOptions.fo_startminimized.value != NAuRuMin )
        {
            AllOptions.fo_startminimized.value = NAuRuMin;
        }
    }

    inline bool IsEnableDelayPanning() const { return AllOptions.fo_delaypan.value; }
    void        SetEnableDelayPanning ( const bool bDelayPanningOn )
    {
        if ( AllOptions.fo_delaypan.value != bDelayPanningOn )
        {
            AllOptions.fo_delaypan.value = bDelayPanningOn;
        }
    }

    inline bool IsIPv6Enabled() const { return AllOptions.fo_ipv6.value; }

    // TODO: make the jam recorder take care of itself (and make a nojamrecorder CONFIG)
    inline bool    GetRecorderInitialised() { return JamController.GetRecorderInitialised(); }
    void           SetEnableRecording ( bool bNewEnableRecording );
    inline QString GetRecorderErrMsg() { return JamController.GetRecorderErrMsg(); }
    inline bool    GetRecordingEnabled() { return JamController.GetRecordingEnabled(); }
    inline void    RequestNewRecording() { JamController.RequestNewRecording(); }
    inline void    SetRecordingDir ( QString newRecordingDir )
    {
        JamController.SetRecordingDir ( newRecordingDir, iServerFrameSizeSamples, AllOptions.fo_norecord.value );
    }
    inline QString GetRecordingDir() { return JamController.GetRecordingDir(); }

signals:
    void Started();
    void Stopped();
    void ClientDisconnected ( const int iChID );
    void SvrRegStatusChanged();
    void AudioFrame ( const int              iChID,
                      const QString          stChName,
                      const CHostAddress     RecHostAddr,
                      const int              iNumAudChan,
                      const CVector<int16_t> vecsData );

    void CLVersionAndOSReceived ( CHostAddress InetAddr, COSUtil::EOpSystemType eOSType, QString strVersion );

    // Settings updates
    void LanguageChanged ( const QString strLanguage, const bool noTranslation );

    // pass through from jam controller
    void RestartRecorder();
    void StopRecorder();
    void RecordingSessionStarted ( QString sessionDir );
    void EndRecorderThread();

public slots:
    void OnNewConnection ( int iChID, int iTotChans, CHostAddress RecHostAddr );

    void OnServerFull ( CHostAddress RecHostAddr );

    void OnProtocolCLMessageReceived ( int iRecID, CVector<uint8_t> vecbyMesBodyData, CHostAddress RecHostAddr );

    void OnProtocolMessageReceived ( int iRecCounter, int iRecID, CVector<uint8_t> vecbyMesBodyData, CHostAddress RecHostAddr );

protected:
    // access functions for actual channels
    bool IsConnected ( const int iChanNum ) { return vecChannels[iChanNum].IsConnected(); }

    int                   FindChannel ( const CHostAddress& CheckAddr, const bool bAllowNew = false );
    void                  InitChannel ( const int iNewChanID, const CHostAddress& InetAddr );
    void                  FreeChannel ( const int iCurChanID );
    void                  DumpChannels ( const QString& title );
    CVector<CChannelInfo> CreateChannelList();

    virtual void CreateAndSendChanListForAllConChannels();
    virtual void CreateAndSendChanListForThisChan ( const int iCurChanID );

    virtual void CreateAndSendChatTextForAllConChannels ( const int iCurChanID, const QString& strChatText );

    virtual void CreateOtherMuteStateChanged ( const int iCurChanID, const int iOtherChanID, const bool bIsMuted );

    virtual void CreateAndSendJitBufMessage ( const int iCurChanID, const int iNNumFra );

    virtual void SendProtMessage ( int iChID, CVector<uint8_t> vecMessage );

    template<unsigned int slotId>
    inline void connectChannelSignalsToServerSlots();

    void WriteHTMLChannelList();
    void WriteHTMLServerQuit();

    static void DecodeReceiveDataBlocks ( CServer* pServer, const int iStartChanCnt, const int iStopChanCnt, const int iNumClients );

    static void MixEncodeTransmitDataBlocks ( CServer* pServer, const int iStartChanCnt, const int iStopChanCnt, const int iNumClients );

    void DecodeReceiveData ( const int iChanCnt, const int iNumClients );

    void MixEncodeTransmitData ( const int iChanCnt, const int iNumClients );

    virtual void customEvent ( QEvent* pEvent );

    void CreateAndSendRecorderStateForAllConChannels();

    bool CreateLevelsForAllConChannels ( const int                       iNumClients,
                                         const CVector<int>&             vecNumAudioChannels,
                                         const CVector<CVector<int16_t>> vecvecsData,
                                         CVector<uint16_t>&              vecLevelsOut );

    int iServerFrameSizeSamples;

    // channel level update frame interval counter
    int iFrameCount;

    // variables needed for multithreading support
    int                        iMaxNumThreads;
    CVector<std::future<void>> Futures;

    std::unique_ptr<CThreadPool> pThreadPool;

    // do not use the vector class since CChannel does not have appropriate
    // copy constructor/operator
    CChannel vecChannels[MAX_NUM_CHANNELS];

    int    iCurNumChannels;
    int    vecChannelOrder[MAX_NUM_CHANNELS];
    QMutex MutexChanOrder;

    CProtocol ConnLessProtocol;
    QMutex    Mutex;
    QMutex    MutexWelcomeMessage;
    bool      bChannelIsNowDisconnected;

    // audio encoder/decoder
    OpusCustomMode*    Opus64Mode[MAX_NUM_CHANNELS];
    OpusCustomEncoder* Opus64EncoderMono[MAX_NUM_CHANNELS];
    OpusCustomDecoder* Opus64DecoderMono[MAX_NUM_CHANNELS];
    OpusCustomEncoder* Opus64EncoderStereo[MAX_NUM_CHANNELS];
    OpusCustomDecoder* Opus64DecoderStereo[MAX_NUM_CHANNELS];
    OpusCustomMode*    OpusMode[MAX_NUM_CHANNELS];
    OpusCustomEncoder* OpusEncoderMono[MAX_NUM_CHANNELS];
    OpusCustomDecoder* OpusDecoderMono[MAX_NUM_CHANNELS];
    OpusCustomEncoder* OpusEncoderStereo[MAX_NUM_CHANNELS];
    OpusCustomDecoder* OpusDecoderStereo[MAX_NUM_CHANNELS];
    CConvBuf<int16_t>  DoubleFrameSizeConvBufIn[MAX_NUM_CHANNELS];
    CConvBuf<int16_t>  DoubleFrameSizeConvBufOut[MAX_NUM_CHANNELS];

    CVector<QString> vstrChatColors;
    CVector<int>     vecChanIDsCurConChan;

    CVector<CVector<float>>   vecvecfGains;
    CVector<CVector<float>>   vecvecfPannings;
    CVector<CVector<int16_t>> vecvecsData;
    CVector<CVector<int16_t>> vecvecsData2;
    CVector<int>              vecNumAudioChannels;
    CVector<int>              vecNumFrameSizeConvBlocks;
    CVector<int>              vecUseDoubleSysFraSizeConvBuf;
    CVector<EAudComprType>    vecAudioComprType;
    CVector<CVector<int16_t>> vecvecsSendData;
    CVector<CVector<float>>   vecvecfIntermediateProcBuf;
    CVector<CVector<uint8_t>> vecvecbyCodedData;

    // HTML file server status
    bool bWriteStatusHTMLFile = false;

    // Server welcome message chat text to send
    QString strWelcomeMessage;

    // Channel levels
    CVector<uint16_t> vecChannelLevels;

    // actual working objects
    CHighPrioSocket Socket;

    CHighPrecisionTimer HighPrecisionTimer;

    // server list
    CServerListManager ServerListManager;

    // jam recorder
    recorder::CJamController JamController;

    // logging
    CServerLogging Logging;

    // messaging
    CSignalHandler* pSignalHandler = CSignalHandler::getSingletonP();

private slots:
    void OnTimer();

    void OnSendCLProtMessage ( CHostAddress InetAddr, CVector<uint8_t> vecMessage );

    void OnCLPingReceived ( CHostAddress InetAddr, int iMs ) { ConnLessProtocol.CreateCLPingMes ( InetAddr, iMs ); }

    void OnCLPingWithNumClientsReceived ( CHostAddress InetAddr, int iMs, int )
    {
        ConnLessProtocol.CreateCLPingWithNumClientsMes ( InetAddr, iMs, GetNumberOfConnectedClients() );
    }

    void OnCLSendEmptyMes ( CHostAddress TargetInetAddr )
    {
        // only send empty message if not a directory server
        if ( !ServerListManager.IsADirectory() )
        {
            ConnLessProtocol.CreateCLEmptyMes ( TargetInetAddr );
        }
    }

    void OnCLReqServerList ( CHostAddress InetAddr ) { ServerListManager.RetrieveAll ( InetAddr ); }

    void OnCLReqVersionAndOS ( CHostAddress InetAddr ) { ConnLessProtocol.CreateCLVersionAndOSMes ( InetAddr ); }

    void OnCLReqConnClientsList ( CHostAddress InetAddr ) { ConnLessProtocol.CreateCLConnClientsListMes ( InetAddr, CreateChannelList() ); }

    void OnCLRegisterServerReceived ( CHostAddress InetAddr, CHostAddress LInetAddr, CServerCoreInfo ServerInfo )
    {
        ServerListManager.Append ( InetAddr, LInetAddr, ServerInfo );
    }

    void OnCLRegisterServerExReceived ( CHostAddress    InetAddr,
                                        CHostAddress    LInetAddr,
                                        CServerCoreInfo ServerInfo,
                                        COSUtil::EOpSystemType,
                                        QString strVersion )
    {
        ServerListManager.Append ( InetAddr, LInetAddr, ServerInfo, strVersion );
    }

    void OnCLRegisterServerResp ( CHostAddress /* unused */, ESvrRegResult eResult ) { ServerListManager.StoreRegistrationResult ( eResult ); }

    void OnCLUnregisterServerReceived ( CHostAddress InetAddr ) { ServerListManager.Remove ( InetAddr ); }

    void OnCLDisconnection ( CHostAddress InetAddr );

    void OnAboutToQuit();

    void OnHandledSignal ( int sigNum );
};

Q_DECLARE_METATYPE ( CVector<int16_t> )
