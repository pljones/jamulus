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

#include <QHostAddress>
#include <QHostInfo>
#include <QString>
#include <QDateTime>
#include <QMutex>
#ifdef USE_OPUS_SHARED_LIB
#    include "opus/opus_custom.h"
#else
#    include "opus_custom.h"
#endif
#include "global.h"
#include "socket.h"
#include "channel.h"
#include "util.h"
#include "buffer.h"
#include "signalhandler.h"

#if defined( _WIN32 ) && !defined( JACK_ON_WINDOWS )
#    include "sound/asio/sound.h"
#else
#    if ( defined( Q_OS_MACX ) ) && !defined( JACK_REPLACES_COREAUDIO )
#        include "sound/coreaudio-mac/sound.h"
#    else
#        if defined( Q_OS_IOS )
#            include "sound/coreaudio-ios/sound.h"
#        else
#            ifdef ANDROID
#                include "sound/oboe/sound.h"
#            else
#                include "sound/jack/sound.h"
#                ifndef JACK_ON_WINDOWS // these headers are not available in Windows OS
#                    include <sched.h>
#                    include <netdb.h>
#                endif
#                include <socket.h>
#            endif
#        endif
#    endif
#endif

/* Definitions ****************************************************************/
// audio in fader range
#define AUD_FADER_IN_MIN    0
#define AUD_FADER_IN_MAX    100
#define AUD_FADER_IN_MIDDLE ( AUD_FADER_IN_MAX / 2 )

// audio reverberation range
#define AUD_REVERB_MAX 100

// default delay period between successive gain updates (ms)
// this will be increased to double the ping time if connected to a distant server
#define DEFAULT_GAIN_DELAY_PERIOD_MS 50

// OPUS number of coded bytes per audio packet
// TODO we have to use new numbers for OPUS to avoid that old CELT packets
// are used in the OPUS decoder (which gives a bad noise output signal).
// Later on when the CELT is completely removed we could set the OPUS
// numbers back to the original CELT values (to reduce network load)

// calculation to get from the number of bytes to the code rate in bps:
// rate [pbs] = Fs / L * N * 8, where
// Fs: sampling rate (SYSTEM_SAMPLE_RATE_HZ)
// L:  number of samples per packet (SYSTEM_FRAME_SIZE_SAMPLES)
// N:  number of bytes per packet (values below)
#define OPUS_NUM_BYTES_MONO_LOW_QUALITY                   12
#define OPUS_NUM_BYTES_MONO_NORMAL_QUALITY                22
#define OPUS_NUM_BYTES_MONO_HIGH_QUALITY                  36
#define OPUS_NUM_BYTES_MONO_LOW_QUALITY_DBLE_FRAMESIZE    25
#define OPUS_NUM_BYTES_MONO_NORMAL_QUALITY_DBLE_FRAMESIZE 45
#define OPUS_NUM_BYTES_MONO_HIGH_QUALITY_DBLE_FRAMESIZE   82

#define OPUS_NUM_BYTES_STEREO_LOW_QUALITY                   24
#define OPUS_NUM_BYTES_STEREO_NORMAL_QUALITY                35
#define OPUS_NUM_BYTES_STEREO_HIGH_QUALITY                  73
#define OPUS_NUM_BYTES_STEREO_LOW_QUALITY_DBLE_FRAMESIZE    47
#define OPUS_NUM_BYTES_STEREO_NORMAL_QUALITY_DBLE_FRAMESIZE 71
#define OPUS_NUM_BYTES_STEREO_HIGH_QUALITY_DBLE_FRAMESIZE   165

#include "options.h"

/* Classes ********************************************************************/
class CClient : public QObject
{
    Q_OBJECT

public:
    CClient();

    virtual ~CClient();

    void Start();
    void Stop();

    double GetLevelForMeterdBLeft() { return SignalLevelMeter.GetLevelForMeterdBLeftOrMono(); }
    double GetLevelForMeterdBRight() { return SignalLevelMeter.GetLevelForMeterdBRight(); }

    bool GetAndResetbJitterBufferOKFlag();

    bool IsIPv6Enabled() const { return AllOptions.fo_ipv6.value; }

    inline QString          GetChannelName() const { return AllOptions.so_name.value; }
    inline QLocale::Country GetChannelCountry() const { return static_cast<QLocale::Country> ( AllOptions.io_country.value ); }
    inline QString          GetChannelCity() const { return AllOptions.so_city.value; }
    inline int              GetChannelInstrument() const { return AllOptions.io_instrument.toInt(); }
    inline ESkillLevel      GetChannelSkillLevel() const { return static_cast<ESkillLevel> ( AllOptions.io_skill.value ); }

    // update channel info at the server after each of these
    // but only if the value changes
    void SetChannelName ( const QString name )
    {
        if ( name.length() <= MAX_LEN_FADER_TAG && name != AllOptions.so_name.value )
        {
            AllOptions.so_name.value = name;
            SetRemoteInfo();
        }
    }
    void SetChannelCountry ( const QLocale::Country country )
    {
        if ( country != AllOptions.io_country.value )
        {
            AllOptions.io_country.value = country;
            SetRemoteInfo();
        }
    }
    void SetChannelCity ( const QString city )
    {
        if ( city.length() <= MAX_LEN_SERVER_CITY && city != AllOptions.so_city.value )
        {
            AllOptions.so_city.value = city;
            SetRemoteInfo();
        }
    }
    void SetChannelInstrument ( const int instrument )
    {
        if ( instrument != AllOptions.io_instrument.value )
        {
            AllOptions.io_instrument.value = instrument;
            SetRemoteInfo();
        }
    }
    void SetChannelSkillLevel ( const ESkillLevel skill )
    {
        if ( skill != AllOptions.io_skill.value )
        {
            AllOptions.io_skill.value = skill;
            SetRemoteInfo();
        }
    }

    inline QString GetLanguage() const { return AllOptions.so_language.value; }
    void           SetLanguage ( const QString value )
    {
        if ( AllOptions.so_language.value != value )
        {
            AllOptions.so_language.value = value;
        }
    }

    inline EAudioQuality GetAudioQuality() const { return static_cast<EAudioQuality> ( AllOptions.io_audioquality.value ); }
    void                 SetAudioQuality ( const EAudioQuality eNAudioQuality );

    inline EAudChanConf GetAudioChannels() const { return static_cast<EAudChanConf> ( AllOptions.io_audiochannels.value ); }
    void                SetAudioChannels ( const EAudChanConf eNAudChanConf );

    inline int GetAudioInFader() const { return AllOptions.io_audfad.toInt(); }
    void       SetAudioInFader ( const int iNV )
    {
        if ( AllOptions.io_audfad.value != iNV )
        {
            AllOptions.io_audfad.value = iNV;
        }
    }

    inline int GetReverbLevel() const { return AllOptions.io_revlev.toInt(); }
    void       SetReverbLevel ( const int iNL )
    {
        if ( AllOptions.io_revlev.value != iNL )
        {
            AllOptions.io_revlev.value = iNL;
        }
    }

    bool IsReverbOnLeftChan() const { return AllOptions.fo_reverblchan.value; }
    void SetReverbOnLeftChan ( const bool bIL )
    {
        if ( AllOptions.fo_reverblchan.value != bIL )
        {
            AllOptions.fo_reverblchan.value = bIL;
            AudioReverb.Clear();
        }
    }

    inline int GetNewClientFaderLevel() const { return AllOptions.io_newclientlevel.toInt(); }
    void       SetNewClientFaderLevel ( const int value )
    {
        if ( AllOptions.io_newclientlevel.value != value )
        {
            AllOptions.io_newclientlevel.value = value;
        }
    }

    bool IsOwnFaderFirst() const { return AllOptions.fo_ownfaderfirst.value; }
    void SetOwnFaderFirst ( const bool value )
    {
        if ( AllOptions.fo_ownfaderfirst.value != value )
        {
            AllOptions.fo_ownfaderfirst.value = value;
        }
    }

    inline int GetNumMixerPanelRows() const { return AllOptions.io_numrowsmixpan.toInt(); }
    void       SetNumMixerPanelRows ( const int numrowsmixpan )
    {
        if ( AllOptions.io_numrowsmixpan.value != numrowsmixpan )
        {
            AllOptions.io_numrowsmixpan.value = numrowsmixpan;
        }
    }

    bool IsAudioAlertsActive() const { return AllOptions.fo_audioalerts.value; }
    void SetAudioAlertsActive ( const bool bNewAudioAlertsActive )
    {
        if ( AllOptions.fo_audioalerts.value != bNewAudioAlertsActive )
        {
            AllOptions.fo_audioalerts.value = bNewAudioAlertsActive;
        }
    }

    inline EChSortType GetChannelSortType() const { return static_cast<EChSortType> ( AllOptions.io_channelsort.value ); }
    void               SetChannelSortType ( const EChSortType value )
    {
        if ( AllOptions.io_channelsort.value != value )
        {
            AllOptions.io_channelsort.value = value;
        }
    }

    inline bool             LoadFaderSettings ( const QString strFileName ) const { return AnyOption::LoadFaderSettings ( strFileName ); }
    inline CVector<int>     GetStoredFaderLevels() const { return AllOptions.vo_storedfaderlevel.value; }
    inline CVector<int>     GetStoredPanValues() const { return AllOptions.vo_storedpanvalue.value; }
    inline CVector<bool>    GetStoredFaderIsSolo() const { return AllOptions.vo_storedfaderissolo.value; }
    inline CVector<bool>    GetStoredFaderIsMute() const { return AllOptions.vo_storedfaderismute.value; }
    inline CVector<int>     GetStoredFaderGroupID() const { return AllOptions.vo_storedgroupid.value; }
    inline CVector<QString> GetStoredFaderTags() const { return AllOptions.vo_storedfadertag.value; }

    int FindStoredFaderTag ( const QString tag ) const
    {
        auto result = std::find ( AllOptions.vo_storedfadertag.value.begin(), AllOptions.vo_storedfadertag.value.end(), tag );
        if ( result != AllOptions.vo_storedfadertag.value.end() )
        {
            return static_cast<int> ( std::distance ( AllOptions.vo_storedfadertag.value.begin(), result ) );
        }
        return -1;
    }

    inline bool SaveFaderSettings ( const QString strFileName ) const { return AnyOption::SaveFaderSettings ( strFileName ); }
    void        SetStoredFaderLevel ( const uint idx, const int faderLevel ) const
    {
        if ( AllOptions.vo_storedfaderlevel.value[idx] != faderLevel )
        {
            AllOptions.vo_storedfaderlevel.value[idx] = faderLevel;
        }
    }
    void SetStoredPanValue ( const uint idx, const int panValue ) const
    {
        if ( AllOptions.vo_storedpanvalue.value[idx] != panValue )
        {
            AllOptions.vo_storedpanvalue.value[idx] = panValue;
        }
    }
    void SetStoredFaderIsSolo ( const uint idx, const bool isSolo ) const
    {
        if ( AllOptions.vo_storedfaderissolo.value[idx] != isSolo )
        {
            AllOptions.vo_storedfaderissolo.value[idx] = isSolo;
        }
    }
    void SetStoredFaderIsMute ( const uint idx, const bool isMute ) const
    {
        if ( AllOptions.vo_storedfaderismute.value[idx] != isMute )
        {
            AllOptions.vo_storedfaderismute.value[idx] = isMute;
        }
    }
    void SetStoredFaderGroupID ( const uint idx, const int groupID ) const
    {
        if ( AllOptions.vo_storedgroupid.value[idx] != groupID )
        {
            AllOptions.vo_storedgroupid.value[idx] = groupID;
        }
    }
    void SetStoredFaderTag ( const uint idx, const QString tag ) const
    {
        if ( AllOptions.vo_storedfadertag.value[idx] != tag )
        {
            AllOptions.vo_storedfadertag.value[idx] = tag;
        }
    }

    void ClearStoredFaderIsSolo() const { AllOptions.vo_storedfaderissolo.value.Reset ( false ); }
    void ClearStoredFaderIsMute() const { AllOptions.vo_storedfaderismute.value.Reset ( false ); }

    int StoreFaderTag ( const QString& tag ) const { return AllOptions.vo_storedfadertag.value.StringFiFoWithCompare ( tag ); }

    inline CVector<QString> GetServerAddresses() const { return AllOptions.vo_serveraddresses.value; }
    void                    SetServerAddresses ( const uint idx, const QString value ) const
    {
        if ( AllOptions.vo_serveraddresses.value[idx] != value )
        {
            AllOptions.vo_serveraddresses.value[idx];
        }
    }

    int StoreServerAddress ( const QString& value ) const { return AllOptions.vo_serveraddresses.value.StringFiFoWithCompare ( value ); }

    inline EDirectoryType GetDirectoryType() const { return static_cast<EDirectoryType> ( AllOptions.io_directorytype.value ); }
    void                  SetDirectoryType ( const EDirectoryType value )
    {
        if ( AllOptions.io_directorytype.value != value )
        {
            AllOptions.io_directorytype.value = value;
        }
    }

    inline uint GetCustomDirectoryIndex() const { return static_cast<uint> ( AllOptions.io_customdirectoryindex.value ); }
    void        SetCustomDirectoryIndex ( const uint value )
    {
        if ( AllOptions.io_customdirectoryindex.value != value )
        {
            AllOptions.io_customdirectoryindex.value = value;
        }
    }

    inline CVector<QString> GetDirectoryAddresses() const { return AllOptions.vo_directoryaddresses.value; }
    void                    SetDirectoryAddresses ( const uint idx, const QString value ) const
    {
        if ( AllOptions.vo_directoryaddresses.value[idx] != value )
        {
            AllOptions.vo_directoryaddresses.value[idx];
        }
    }

    int  StoreDirectoryAddress ( const QString& value ) const { return AllOptions.vo_directoryaddresses.value.StringFiFoWithCompare ( value ); }
    int  RemoveDirectoryAddress ( const QString& value ) const { return AllOptions.vo_directoryaddresses.value.Remove ( value ); }
    void RemoveDirectoryAt ( const size_t posn ) const { AllOptions.vo_directoryaddresses.value[posn] = ""; }

    void        SetDoAutoSockBufSize ( const bool bValue );
    inline bool GetDoAutoSockBufSize() const { return Channel.GetDoAutoSockBufSize(); }

    inline int GetSockBufNumFrames() const { return Channel.GetSockBufNumFrames(); }
    void       SetSockBufNumFrames ( const int iNumBlocks, const bool bPreserve = false )
    {
        if ( AllOptions.io_jitbuf.value == iNumBlocks )
        {
            return;
        }
        AllOptions.io_jitbuf.value = iNumBlocks;
        Channel.SetSockBufNumFrames ( iNumBlocks, bPreserve );
    }

    inline int GetServerSockBufNumFrames() const { return AllOptions.io_jitbufserver.toInt(); }
    void       SetServerSockBufNumFrames ( const int iNumBlocks )
    {
        if ( AllOptions.io_jitbufserver.value == iNumBlocks )
        {
            return;
        }
        AllOptions.io_jitbufserver.value = iNumBlocks;

        // if auto setting is disabled, inform the server about the new size
        if ( !GetDoAutoSockBufSize() )
        {
            Channel.CreateJitBufMes ( AllOptions.io_jitbufserver.toInt() );
        }
    }

    bool IsShowAnalyzerConsole() const { return AllOptions.fo_showanalyzerconsole.value; }

    inline EGUIDesign GetGUIDesign() const { return static_cast<EGUIDesign> ( AllOptions.io_guidesign.value ); }
    void              SetGUIDesign ( const EGUIDesign eNewDesign )
    {
        if ( AllOptions.io_guidesign.value != eNewDesign )
        {
            AllOptions.io_guidesign.value = eNewDesign;
        }
    }

    inline EMeterStyle GetMeterStyle() const { return static_cast<EMeterStyle> ( AllOptions.io_meterstyle.value ); }
    void               SetMeterStyle ( const EMeterStyle eNewMeterStyle )
    {
        if ( AllOptions.io_meterstyle.value != eNewMeterStyle )
        {
            AllOptions.io_meterstyle.value = eNewMeterStyle;
        }
    }

    bool IsEnableFeedbackDetection() { return AllOptions.fo_detectfeedback.value; }
    void SetEnableFeedbackDetection ( const bool detectfeedback )
    {
        if ( AllOptions.fo_detectfeedback.value != detectfeedback )
        {
            AllOptions.fo_detectfeedback.value = detectfeedback;
        }
    }

    inline int GetInputBoost() { return AllOptions.io_inputboost.toInt(); }
    void       SetInputBoost ( const int iNewBoost )
    {
        if ( AllOptions.io_inputboost.value != iNewBoost )
        {
            AllOptions.io_inputboost.value = iNewBoost;
        }
    }

    bool MuteOutStream() { return AllOptions.fo_mutestream.value; }
    void SetMuteOutStream ( const bool bDoMute )
    {
        if ( AllOptions.fo_mutestream.value != bDoMute )
        {
            AllOptions.fo_mutestream.value = bDoMute;
        }
    }

    inline bool GetEnableOPUS64() { return AllOptions.fo_enableopussmall.value; }
    void        SetEnableOPUS64 ( const bool eNEnableOPUS64 );

    inline QString GetClientName() const { return AllOptions.so_clientname.value; }

    inline QString GetMIDIControls() const { return AllOptions.so_ctrlmidich.value; }

    inline QString GetConnectServer() const { return AllOptions.so_connect.value; }

    // Client GUI window locations
    inline QByteArray GetMainWindowGeometry() const { return AllOptions.bo_winposmain.value; }
    inline QByteArray GetSettingsWindowGeometry() const { return AllOptions.bo_winposset.value; }
    inline QByteArray GetChatWindowGeometry() const { return AllOptions.bo_winposchat.value; }
    inline QByteArray GetConnectWindowGeometry() const { return AllOptions.bo_winposcon.value; }

    void SetMainWindowGeometry ( const QByteArray value )
    {
        if ( AllOptions.bo_winposmain.value != value )
        {
            AllOptions.bo_winposmain.value = value;
        }
    }
    void SetSettingsWindowGeometry ( const QByteArray value )
    {
        if ( AllOptions.bo_winposset.value != value )
        {
            AllOptions.bo_winposset.value = value;
        }
    }
    void SetChatWindowGeometry ( const QByteArray value )
    {
        if ( AllOptions.bo_winposchat.value != value )
        {
            AllOptions.bo_winposchat.value = value;
        }
    }
    void SetConnectWindowGeometry ( const QByteArray value )
    {
        if ( AllOptions.bo_winposcon.value != value )
        {
            AllOptions.bo_winposcon.value = value;
        }
    }

    // Client GUI window visibility
    bool IsSettingsWindowVisible() const { return AllOptions.fo_winvisset.value; }
    bool IsChatWindowVisible() const { return AllOptions.fo_winvischat.value; }
    bool IsConnectWindowVisible() const { return AllOptions.fo_winviscon.value; }

    void SetSettingsWindowVisible ( const bool value )
    {
        if ( AllOptions.fo_winvisset.value != value )
        {
            AllOptions.fo_winvisset.value = value;
        }
    }
    void SetChatWindowVisible ( const bool value )
    {
        if ( AllOptions.fo_winvischat.value != value )
        {
            AllOptions.fo_winvischat.value = value;
        }
    }
    void SetConnectWindowVisible ( const bool value )
    {
        if ( AllOptions.fo_winviscon.value != value )
        {
            AllOptions.fo_winviscon.value = value;
        }
    }

    // Client Settings GUI selected tab - unused?
    inline int GetSettingsTab() const { return AllOptions.io_settingstab.toInt(); }
    void       SetSettingsTab ( const int value )
    {
        if ( AllOptions.io_settingstab.value != value )
        {
            AllOptions.io_settingstab.value = value;
        }
    }

    // Connect GUI show all musicians checkbox
    bool IsShowAllMusicians() const { return AllOptions.fo_connectdlgshowallmusicians.value; }
    void SetShowAllMusicians ( const bool value )
    {
        if ( AllOptions.fo_connectdlgshowallmusicians.value != value )
        {
            AllOptions.fo_connectdlgshowallmusicians.value = value;
        }
    }

    bool IsShowAllServers() const { return AllOptions.fo_showallservers.value; }

    inline int GetSndCrdPrefFrameSizeFactor() { return AllOptions.io_prefsndcrdbufidx.toInt(); }
    void       SetSndCrdPrefFrameSizeFactor ( const int iNewFactor );

    // sound card device selection
    QStringList GetSndCrdDevNames() { return Sound.GetDevNames(); }

    QString SetSndCrdDev ( const QString strNewDev );
    QString GetSndCrdDev() { return Sound.GetDev(); }
    void    OpenSndCrdDriverSetup() { Sound.OpenDriverSetup(); }

    // sound card channel selection
    int     GetSndCrdNumInputChannels() { return Sound.GetNumInputChannels(); }
    int     GetSndCrdLeftInputChannel() { return Sound.GetLeftInputChannel(); }
    void    SetSndCrdLeftInputChannel ( const int iNewChan );
    int     GetSndCrdRightInputChannel() { return Sound.GetRightInputChannel(); }
    void    SetSndCrdRightInputChannel ( const int iNewChan );
    QString GetSndCrdInputChannelName ( const int iDiD ) { return Sound.GetInputChannelName ( iDiD ); }

    int     GetSndCrdNumOutputChannels() { return Sound.GetNumOutputChannels(); }
    int     GetSndCrdLeftOutputChannel() { return Sound.GetLeftOutputChannel(); }
    void    SetSndCrdLeftOutputChannel ( const int iNewChan );
    int     GetSndCrdRightOutputChannel() { return Sound.GetRightOutputChannel(); }
    void    SetSndCrdRightOutputChannel ( const int iNewChan );
    QString GetSndCrdOutputChannelName ( const int iDiD ) { return Sound.GetOutputChannelName ( iDiD ); }

    int GetSndCrdActualMonoBlSize()
    {
        // the actual sound card mono block size depends on whether a
        // sound card conversion buffer is used or not
        if ( bSndCrdConversionBufferRequired )
        {
            return iSndCardMonoBlockSizeSamConvBuff;
        }
        else
        {
            return iMonoBlockSizeSam;
        }
    }
    int GetSystemMonoBlSize() { return iMonoBlockSizeSam; }
    int GetSndCrdConvBufAdditionalDelayMonoBlSize()
    {
        if ( bSndCrdConversionBufferRequired )
        {
            // by introducing the conversion buffer we also introduce additional
            // delay which equals the "internal" mono buffer size
            return iMonoBlockSizeSam;
        }
        else
        {
            return 0;
        }
    }

    bool GetFraSiFactPrefSupported() { return bFraSiFactPrefSupported; }
    bool GetFraSiFactDefSupported() { return bFraSiFactDefSupported; }
    bool GetFraSiFactSafeSupported() { return bFraSiFactSafeSupported; }

    bool SetServerAddr ( QString strNAddr );
    bool IsConnected() { return Channel.IsConnected(); }
    int  GetUploadRateKbps() { return Channel.GetUploadRateKbps(); }

    bool IsRunning() { return Sound.IsRunning(); }
    bool IsCallbackEntered() const { return Sound.IsCallbackEntered(); }

    void SetRemoteChanGain ( const int iId, const float fGain, const bool bIsMyOwnFader );
    void OnTimerRemoteChanGain();
    void StartDelayTimer();

    void SetRemoteChanPan ( const int iId, const float fPan ) { Channel.SetRemoteChanPan ( iId, fPan ); }

    void CreateChatTextMes ( const QString& strChatText ) { Channel.CreateChatTextMes ( strChatText ); }

    void CreateCLPingMes() { ConnLessProtocol.CreateCLPingMes ( Channel.GetAddress(), PreparePingMessage() ); }

    void CreateCLServerListPingMes ( const CHostAddress& InetAddr )
    {
        ConnLessProtocol.CreateCLPingWithNumClientsMes ( InetAddr, PreparePingMessage(), 0 /* dummy */ );
    }

    void CreateCLServerListReqVerAndOSMes ( const CHostAddress& InetAddr ) { ConnLessProtocol.CreateCLReqVersionAndOSMes ( InetAddr ); }

    void CreateCLServerListReqConnClientsListMes ( const CHostAddress& InetAddr ) { ConnLessProtocol.CreateCLReqConnClientsListMes ( InetAddr ); }

    void CreateCLReqServerListMes ( const CHostAddress& InetAddr ) { ConnLessProtocol.CreateCLReqServerListMes ( InetAddr ); }

    int EstimatedOverallDelay ( const int iPingTimeMs );

    void GetBufErrorRates ( CVector<double>& vecErrRates, double& dLimit, double& dMaxUpLimit )
    {
        Channel.GetBufErrorRates ( vecErrRates, dLimit, dMaxUpLimit );
    }

    QString GetServerAddr();

protected:
    // Constructor helpers
    void opusSetup();
    void soundCardSetup();
    void channelSetup();
    void signalsToSlots();
    void requestLatestVersion();

    // callback function must be static, otherwise it does not work
    static void AudioCallback ( CVector<short>& psData, void* arg );

    void Init();
    void ProcessSndCrdAudioData ( CVector<short>& vecsStereoSndCrd );
    void ProcessAudioDataIntern ( CVector<short>& vecsStereoSndCrd );

    int  PreparePingMessage();
    int  EvaluatePingMessage ( const int iMs );
    void CreateServerJitterBufferMessage();

    void SetRemoteInfo()
    {
        Channel.SetRemoteInfo ( CChannelCoreInfo ( AllOptions.so_name.value,
                                                   static_cast<QLocale::Country> ( AllOptions.io_country.value ),
                                                   AllOptions.so_city.value,
                                                   AllOptions.io_instrument.toInt(),
                                                   static_cast<ESkillLevel> ( AllOptions.io_skill.value ) ) );
    }

    // only one channel is needed for client application
    CChannel  Channel;
    CProtocol ConnLessProtocol;

    // audio encoder/decoder
    OpusCustomMode*    Opus64Mode          = nullptr;
    OpusCustomEncoder* Opus64EncoderMono   = nullptr;
    OpusCustomDecoder* Opus64DecoderMono   = nullptr;
    OpusCustomEncoder* Opus64EncoderStereo = nullptr;
    OpusCustomDecoder* Opus64DecoderStereo = nullptr;
    OpusCustomMode*    OpusMode            = nullptr;
    OpusCustomEncoder* OpusEncoderMono     = nullptr;
    OpusCustomDecoder* OpusDecoderMono     = nullptr;
    OpusCustomEncoder* OpusEncoderStereo   = nullptr;
    OpusCustomDecoder* OpusDecoderStereo   = nullptr;
    OpusCustomEncoder* CurOpusEncoder      = nullptr;
    OpusCustomDecoder* CurOpusDecoder      = nullptr;

    EAudComprType eAudioCompressionType = CT_OPUS;
    int           iCeltNumCodedBytes    = OPUS_NUM_BYTES_MONO_LOW_QUALITY;
    int           iOPUSFrameSizeSamples = DOUBLE_SYSTEM_FRAME_SIZE_SAMPLES;

    int                    iNumAudioChannels      = 1;
    bool                   bIsInitializationPhase = true;
    float                  fMuteOutStreamGain     = 1.0f;
    CVector<unsigned char> vecCeltData;
    CVector<uint8_t>       vecbyNetwData;

    CHighPrioSocket         Socket;
    CSound                  Sound;
    CStereoSignalLevelMeter SignalLevelMeter;
    CAudioReverb            AudioReverb;

    int iSndCrdFrameSizeFactor = FRAME_SIZE_FACTOR_DEFAULT;

    bool             bSndCrdConversionBufferRequired  = false;
    int              iSndCardMonoBlockSizeSamConvBuff = 0;
    CBuffer<int16_t> SndCrdConversionBufferIn;
    CBuffer<int16_t> SndCrdConversionBufferOut;
    CVector<int16_t> vecDataConvBuf;
    CVector<int16_t> vecsStereoSndCrdMuteStream;
    CVector<int16_t> vecZeros;

    bool bFraSiFactPrefSupported = false;
    bool bFraSiFactDefSupported  = false;
    bool bFraSiFactSafeSupported = false;

    int iMonoBlockSizeSam;
    int iStereoBlockSizeSam;

    bool   bJitterBufferOK = true;
    QMutex MutexDriverReinit;

    // for ping measurement
    QElapsedTimer PreciseTime;

    // for gain rate limiting
    QMutex MutexGain;
    QTimer TimerGain;
    int    minGainId;
    int    maxGainId;
    float  oldGain[MAX_NUM_CHANNELS];
    float  newGain[MAX_NUM_CHANNELS];
    int    iCurPingTime;

    CSignalHandler* pSignalHandler;

protected slots:
    void OnHandledSignal ( int sigNum );
    void OnSendProtMessage ( CVector<uint8_t> vecMessage );
    void OnInvalidPacketReceived ( CHostAddress RecHostAddr );

    void OnDetectedCLMessage ( CVector<uint8_t> vecbyMesBodyData, int iRecID, CHostAddress RecHostAddr );

    void OnReqJittBufSize() { CreateServerJitterBufferMessage(); }
    void OnJittBufSizeChanged ( int iNewJitBufSize );
    void OnReqChanInfo()
    {
        Channel.SetRemoteInfo ( CChannelCoreInfo ( AllOptions.so_name.value,
                                                   static_cast<QLocale::Country> ( AllOptions.io_country.value ),
                                                   AllOptions.so_city.value,
                                                   AllOptions.io_instrument.toInt(),
                                                   static_cast<ESkillLevel> ( AllOptions.io_skill.value ) ) );
    }
    void OnNewConnection();
    void OnCLDisconnection ( CHostAddress InetAddr )
    {
        if ( InetAddr == Channel.GetAddress() )
        {
            emit Disconnected();
        }
    }
    void OnCLVersionAndOSReceived ( CHostAddress InetAddr, COSUtil::EOpSystemType eOSType, QString strVersion );
    void OnCLPingReceived ( CHostAddress InetAddr, int iMs );

    void OnSendCLProtMessage ( CHostAddress InetAddr, CVector<uint8_t> vecMessage );

    void OnCLPingWithNumClientsReceived ( CHostAddress InetAddr, int iMs, int iNumClients );

    void OnSndCrdReinitRequest ( int iSndCrdResetType );
    void OnControllerInFaderLevel ( int iChannelIdx, int iValue );
    void OnControllerInPanValue ( int iChannelIdx, int iValue );
    void OnControllerInFaderIsSolo ( int iChannelIdx, bool bIsSolo );
    void OnControllerInFaderIsMute ( int iChannelIdx, bool bIsMute );
    void OnControllerInMuteMyself ( bool bMute );
    void OnClientIDReceived ( int iChanID );
    void OnConClientListMesReceived ( CVector<CChannelInfo> vecChanInfo );

signals:
    void ConClientListMesReceived ( CVector<CChannelInfo> vecChanInfo );
    void ChatTextReceived ( QString strChatText );
    void ClientIDReceived ( int iChanID );
    void MuteStateHasChangedReceived ( int iChanID, bool bIsMuted );
    void LicenceRequired ( ELicenceType eLicenceType );
    void VersionAndOSReceived ( COSUtil::EOpSystemType eOSType, QString strVersion );
    void PingTimeReceived ( int iPingTime );
    void RecorderStateReceived ( ERecorderState eRecorderState );

    void CLServerListReceived ( CHostAddress InetAddr, CVector<CServerInfo> vecServerInfo );

    void CLRedServerListReceived ( CHostAddress InetAddr, CVector<CServerInfo> vecServerInfo );

    void CLConnClientsListMesReceived ( CHostAddress InetAddr, CVector<CChannelInfo> vecChanInfo );

    void CLPingTimeWithNumClientsReceived ( CHostAddress InetAddr, int iPingTime, int iNumClients );

    void CLVersionAndOSReceived ( CHostAddress InetAddr, COSUtil::EOpSystemType eOSType, QString strVersion );

    void CLChannelLevelListReceived ( CHostAddress InetAddr, CVector<uint16_t> vecLevelList );

    void Disconnected();
    void SoundDeviceChanged ( QString strError );
    void ControllerInFaderLevel ( int iChannelIdx, int iValue );
    void ControllerInPanValue ( int iChannelIdx, int iValue );
    void ControllerInFaderIsSolo ( int iChannelIdx, bool bIsSolo );
    void ControllerInFaderIsMute ( int iChannelIdx, bool bIsMute );
    void ControllerInMuteMyself ( bool bMute );
};
