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

#include "clientdlg.h"

/*

    For each widget:

        set the widget translation strings (any label and the actual control)

        set the widget value to the client value

        connect [Qt Widget]->onSomeSignal to
            (lambda)(if the widget value != the client value, set the client value to the widget value)

        connect Client->onSomeSignal to
            (lambda)(if the widget value != the client value, set the widget value to the client value)

        connect [Qt Widget]->onSomeOtherSignal to
            method that runs multiple steps (load / save settings, etc) or is used in more than one place (connect), etc...

*/

/* Implementation *************************************************************/
CClientDlg::CClientDlg ( CClient& client, QWidget* parent ) :
    CBaseDlg ( parent, Qt::Window ),
    ClientSettingsDlg ( client, parent ),
    ChatDlg ( client, parent ),
    ConnectDlg ( client, parent ), // TODO: Something is wrong in here
    AnalyzerConsole ( client, parent ),
    Client ( client )
{
    // auto-generated - always call this:
    setupUi ( this );

    // There appears to be no way to initialise this when the instance gets constructed
    // so do it here
    MainMixerBoard->SetClient ( &Client );

    createUi();
    makeConnections();
    startTimers();
    restoreWindowGeometry();

    // init status label
    UpdateDisplay();

    if ( Client.IsRunning() )
    {
        SetConnectedGUI ( Client.GetConnectServer() );
    }
}

void CClientDlg::createUi()
{
    // Main menu bar -----------------------------------------------------------
    QMenuBar* pMenu = new QMenuBar ( this );

    // Now tell the layout about the menu
    layout()->setMenuBar ( pMenu );

    // File menu  --------------------------------------------------------------
    {
        QMenu* pFileMenu = new QMenu ( tr ( "&File" ), this );
        pMenu->addMenu ( pFileMenu );

        pFileMenu->addAction ( tr ( "&Connection Setup..." ), this, SLOT ( OnOpenConnectionSetupDialog() ), QKeySequence ( Qt::CTRL + Qt::Key_C ) );

        pFileMenu->addSeparator();

        pFileMenu->addAction ( tr ( "&Load Mixer Channels Setup..." ), this, SLOT ( OnLoadChannelSetup() ) );

        pFileMenu->addAction ( tr ( "&Save Mixer Channels Setup..." ), this, SLOT ( OnSaveChannelSetup() ) );

        pFileMenu->addSeparator();

        pFileMenu->addAction ( tr ( "E&xit" ), this, SLOT ( close() ), QKeySequence ( Qt::CTRL + Qt::Key_Q ) );
    }

    // Edit menu  --------------------------------------------------------------
    {
        QMenu* pEditMenu = new QMenu ( tr ( "&Edit" ), this );
        pMenu->addMenu ( pEditMenu );

        pEditMenu->addAction ( tr ( "Clear &All Stored Solo and Mute Settings" ), this, SLOT ( OnClearAllStoredSoloMuteSettings() ) );

        pEditMenu->addAction ( tr ( "Set All Faders to New Client &Level" ),
                               this,
                               SLOT ( OnSetAllFadersToNewClientLevel() ),
                               QKeySequence ( Qt::CTRL + Qt::Key_L ) );

        pEditMenu->addAction ( tr ( "Auto-Adjust all &Faders" ), this, SLOT ( OnAutoAdjustAllFaderLevels() ), QKeySequence ( Qt::CTRL + Qt::Key_F ) );
    }

    // View menu  --------------------------------------------------------------
    {
        QMenu* pViewMenu = new QMenu ( tr ( "&View" ), this );
        pMenu->addMenu ( pViewMenu );

        // own fader first option: works from server version 3.5.5 which supports sending client ID back to client
        QAction* OwnFaderFirstAction =
            pViewMenu->addAction ( tr ( "O&wn Fader First" ), this, SLOT ( OnOwnFaderFirst() ), QKeySequence ( Qt::CTRL + Qt::Key_W ) );

        pViewMenu->addSeparator();

        QAction* NoSortAction =
            pViewMenu->addAction ( tr ( "N&o User Sorting" ), this, SLOT ( OnNoSortChannels() ), QKeySequence ( Qt::CTRL + Qt::Key_O ) );

        QAction* ByNameAction =
            pViewMenu->addAction ( tr ( "Sort Users by &Name" ), this, SLOT ( OnSortChannelsByName() ), QKeySequence ( Qt::CTRL + Qt::Key_N ) );

        QAction* ByInstrAction = pViewMenu->addAction ( tr ( "Sort Users by &Instrument" ),
                                                        this,
                                                        SLOT ( OnSortChannelsByInstrument() ),
                                                        QKeySequence ( Qt::CTRL + Qt::Key_I ) );

        QAction* ByGroupAction =
            pViewMenu->addAction ( tr ( "Sort Users by &Group" ), this, SLOT ( OnSortChannelsByGroupID() ), QKeySequence ( Qt::CTRL + Qt::Key_G ) );

        QAction* ByCityAction =
            pViewMenu->addAction ( tr ( "Sort Users by &City" ), this, SLOT ( OnSortChannelsByCity() ), QKeySequence ( Qt::CTRL + Qt::Key_T ) );

        OwnFaderFirstAction->setCheckable ( true );
        OwnFaderFirstAction->setChecked ( Client.IsOwnFaderFirst() );

        // the sorting menu entries shall be checkable and exclusive
        QActionGroup* SortActionGroup = new QActionGroup ( this );
        SortActionGroup->setExclusive ( true );
        NoSortAction->setCheckable ( true );
        SortActionGroup->addAction ( NoSortAction );
        ByNameAction->setCheckable ( true );
        SortActionGroup->addAction ( ByNameAction );
        ByInstrAction->setCheckable ( true );
        SortActionGroup->addAction ( ByInstrAction );
        ByGroupAction->setCheckable ( true );
        SortActionGroup->addAction ( ByGroupAction );
        ByCityAction->setCheckable ( true );
        SortActionGroup->addAction ( ByCityAction );

        switch ( static_cast<EChSortType> ( Client.GetChannelSortType() ) )
        {
        case ST_NO_SORT:
            NoSortAction->setChecked ( true );
            break;
        case ST_BY_NAME:
            ByNameAction->setChecked ( true );
            break;
        case ST_BY_INSTRUMENT:
            ByInstrAction->setChecked ( true );
            break;
        case ST_BY_GROUPID:
            ByGroupAction->setChecked ( true );
            break;
        case ST_BY_CITY:
            ByCityAction->setChecked ( true );
            break;
        }

        pViewMenu->addSeparator();

        pViewMenu->addAction ( tr ( "C&hat..." ), this, SLOT ( OnOpenChatDialog() ), QKeySequence ( Qt::CTRL + Qt::Key_H ) );

        // optionally show analyzer console entry
        if ( Client.IsShowAnalyzerConsole() )
        {
            pViewMenu->addSeparator();

            pViewMenu->addAction ( tr ( "&Analyzer Console..." ), this, SLOT ( OnOpenAnalyzerConsole() ) );
        }
    }

    // Settings menu  ----------------------------------------------------------
    {
        QMenu* pSettingsMenu = new QMenu ( tr ( "Sett&ings" ), this );
        pMenu->addMenu ( pSettingsMenu );

        pSettingsMenu->addAction ( tr ( "My &Profile..." ), this, SLOT ( OnOpenUserProfileSettings() ), QKeySequence ( Qt::CTRL + Qt::Key_P ) );

        pSettingsMenu->addAction ( tr ( "Audio/Network &Settings..." ),
                                   this,
                                   SLOT ( OnOpenAudioNetSettings() ),
                                   QKeySequence ( Qt::CTRL + Qt::Key_S ) );

        pSettingsMenu->addAction ( tr ( "A&dvanced Settings..." ), this, SLOT ( OnOpenAdvancedSettings() ), QKeySequence ( Qt::CTRL + Qt::Key_D ) );
    }

    // Help menu  --------------------------------------------------------------
    pMenu->addMenu ( new CHelpMenu ( true, this ) );

    // Add help text to controls -----------------------------------------------

    // reverberation level
    {
        QString strAudReverb = "<b>" + tr ( "Reverb effect" ) + ":</b> " +
                               tr ( "Reverb can be applied to one local mono audio channel or to both "
                                    "channels in stereo mode. The mono channel selection and the "
                                    "reverb level can be modified. For example, if "
                                    "a microphone signal is fed in to the right audio channel of the "
                                    "sound card and a reverb effect needs to be applied, set the "
                                    "channel selector to right and move the fader upwards until the "
                                    "desired reverb level is reached." );

        lblAudioReverb->setWhatsThis ( strAudReverb );
        sldAudioReverb->setWhatsThis ( strAudReverb );

        sldAudioReverb->setAccessibleName ( tr ( "Reverb effect level setting" ) );
    }

    // reverberation channel selection
    {
        QString strRevChanSel = "<b>" + tr ( "Reverb Channel Selection" ) + ":</b> " +
                                tr ( "With these radio buttons the audio input channel on which the "
                                     "reverb effect is applied can be chosen. Either the left "
                                     "or right input channel can be selected." );

        rbtReverbSelL->setWhatsThis ( strRevChanSel );
        rbtReverbSelL->setAccessibleName ( tr ( "Left channel selection for reverb" ) );
        rbtReverbSelR->setWhatsThis ( strRevChanSel );
        rbtReverbSelR->setAccessibleName ( tr ( "Right channel selection for reverb" ) );
    }

    // input level meter
    {
        QString strInpLevH = "<b>" + tr ( "Input Level Meter" ) + ":</b> " +
                             tr ( "This shows "
                                  "the level of the two stereo channels "
                                  "for your audio input." ) +
                             "<br>" +
                             tr ( "Make sure not to clip the input signal to avoid distortions of the "
                                  "audio signal." );

        QString strInpLevHTT = tr ( "If the application "
                                    "is connected to a server and "
                                    "you play your instrument/sing into the microphone, the VU "
                                    "meter should flicker. If this is not the case, you have "
                                    "probably selected the wrong input channel (e.g. 'line in' instead "
                                    "of the microphone input) or set the input gain too low in the "
                                    "(Windows) audio mixer." ) +
                               "<br>" +
                               tr ( "For proper usage of the "
                                    "application, you should not hear your singing/instrument through "
                                    "the loudspeaker or your headphone when the software is not connected."
                                    "This can be achieved by muting your input audio channel in the "
                                    "Playback mixer (not the Recording mixer!)." ) +
                               TOOLTIP_COM_END_TEXT;

        QString strInpLevHAccText  = tr ( "Input level meter" );
        QString strInpLevHAccDescr = tr ( "Simulates an analog LED level meter." );

        lblInputLEDMeter->setWhatsThis ( strInpLevH );
        lblLevelMeterLeft->setWhatsThis ( strInpLevH );
        lblLevelMeterRight->setWhatsThis ( strInpLevH );

        lbrInputLevelL->setWhatsThis ( strInpLevH );
        lbrInputLevelL->setAccessibleName ( strInpLevHAccText );
        lbrInputLevelL->setAccessibleDescription ( strInpLevHAccDescr );
        lbrInputLevelL->setToolTip ( strInpLevHTT );
        lbrInputLevelL->setEnabled ( false );

        lbrInputLevelR->setWhatsThis ( strInpLevH );
        lbrInputLevelR->setAccessibleName ( strInpLevHAccText );
        lbrInputLevelR->setAccessibleDescription ( strInpLevHAccDescr );
        lbrInputLevelR->setToolTip ( strInpLevHTT );
        lbrInputLevelR->setEnabled ( false );
    }

    // delay LED
    {/* no longer shown in client
        QString strLEDDelay = "<b>" + tr ( "Delay Status LED" ) + ":</b> " + tr ( "Shows the current audio delay status:" ) +
                              "<ul>"
                              "<li>"
                              "<b>" +
                              tr ( "Green" ) + ":</b> " +
                              tr ( "The delay is perfect for a jam session." ) +
                              "</li>"
                              "<li>"
                              "<b>" +
                              tr ( "Yellow" ) + ":</b> " +
                              tr ( "A session is still possible but it may be harder to play." ) +
                              "</li>"
                              "<li>"
                              "<b>" +
                              tr ( "Red" ) + ":</b> " +
                              tr ( "The delay is too large for jamming." ) +
                              "</li>"
                              "</ul>";

        lblDelay->setWhatsThis ( strLEDDelay );
        ledDelay->setWhatsThis ( strLEDDelay );
        ledDelay->setToolTip ( tr ( "If this LED indicator turns red, "
                                    "you will not have much fun using %1." )
                                   .arg ( APP_NAME ) +
                               TOOLTIP_COM_END_TEXT );

        ledDelay->setAccessibleName ( tr ( "Delay status LED indicator" ) );
    */}

    // Jitter LED
    {
        QString strLEDBuffers = "<b>" + tr ( "Local Jitter Buffer Status LED" ) + ":</b> " +
                                tr ( "The local jitter buffer status LED shows the current audio/streaming "
                                     "status. If the light is red, the audio stream is interrupted. "
                                     "This is caused by one of the following problems:" ) +
                                "<ul>"
                                "<li>" +
                                tr ( "The network jitter buffer is not large enough for the current "
                                     "network/audio interface jitter." ) +
                                "</li>"
                                "<li>" +
                                tr ( "The sound card's buffer delay (buffer size) is too small "
                                     "(see Settings window)." ) +
                                "</li>"
                                "<li>" +
                                tr ( "The upload or download stream rate is too high for your "
                                     "internet bandwidth." ) +
                                "</li>"
                                "<li>" +
                                tr ( "The CPU of the client or server is at 100%." ) +
                                "</li>"
                                "</ul>";

        lblBuffers->setWhatsThis ( strLEDBuffers );
        ledBuffers->setWhatsThis ( strLEDBuffers );
        ledBuffers->setToolTip ( tr ( "If this LED indicator turns red, "
                                      "the audio stream is interrupted." ) +
                                 TOOLTIP_COM_END_TEXT );

        ledBuffers->setAccessibleName ( tr ( "Local Jitter Buffer status LED indicator" ) );
    }

    // current connection status parameter
    {
        QString strConnStats = "<b>" + tr ( "Current Connection Status" ) + ":</b> " +
                               tr ( "The Ping Time is the time required for the audio "
                                    "stream to travel from the client to the server and back again. This "
                                    "delay is introduced by the network and should be about "
                                    "20-30 ms. If this delay is higher than about 50 ms, your distance to "
                                    "the server is too large or your internet connection is not "
                                    "sufficient." ) +
                               "<br>" +
                               tr ( "Overall Delay is calculated from the current Ping Time and the "
                                    "delay introduced by the current buffer settings." );

        lblPing->setWhatsThis ( strConnStats );
        lblPingVal->setWhatsThis ( strConnStats );
        lblDelay->setWhatsThis ( strConnStats );
        lblDelayVal->setWhatsThis ( strConnStats );
        lblPingVal->setText ( "---" );
        lblPingUnit->setText ( "" );
        lblDelayVal->setText ( "---" );
        lblDelayUnit->setText ( "" );
    }

    // TODO:
    // WhatsThis and AccessibleName for checkboxes
    {
        // ----
        // WhatsThis and Accessibility for checkboxes
        // ----
    }

    // connect/disconnect button
    {
        butConnect->setWhatsThis ( "<b>" + tr ( "Connect/Disconnect Button" ) + ":</b> " +
                                   tr ( "Opens a dialog where you can select a server to connect to. "
                                        "If you are connected, pressing this button will end the session." ) );

        butConnect->setAccessibleName ( tr ( "Connect and disconnect toggle button" ) );
    }

    // ---------------------------------------------------------------------------------

    // init GUI design
    UpdateGUIDesign();

    // MeterStyle init
    UpdateMeterStyle();

    // init mixer board
    MainMixerBoard->SetGUIDesign ( Client.GetGUIDesign() );
    MainMixerBoard->SetMeterStyle ( Client.GetMeterStyle() );
    MainMixerBoard->SetFaderSorting ( Client.GetChannelSortType() );
    MainMixerBoard->SetNumMixerPanelRows ( Client.GetNumMixerPanelRows() );
    MainMixerBoard->SetMIDICtrlUsed ( !Client.GetMIDIControls().isEmpty() );
    MainMixerBoard->HideAll();

    // init connection button text
    butConnect->setText ( Client.IsConnected() ? tr ( "&Disconnect" ) : tr ( "C&onnect" ) );

    // init input level meter bars
    lbrInputLevelL->SetValue ( 0 );
    lbrInputLevelR->SetValue ( 0 );

    // init status LEDs
    ledBuffers->Reset();
    ledDelay->Reset();

    // init audio reverberation
    sldAudioReverb->setRange ( 0, AUD_REVERB_MAX );
    sldAudioReverb->setTickInterval ( AUD_REVERB_MAX / 5 );
    sldAudioReverb->setValue ( Client.GetReverbLevel() );

    chbLocalMute->setCheckState ( Client.MuteOutStream() ? Qt::Checked : Qt::Unchecked );

    // init reverb channel
    UpdateRevSelection();

    // TODO:
    // get the client value...
    // set window title (with no clients connected -> "0")
    SetMyWindowTitle ( 0 );

    // prepare Mute Myself info label (invisible by default)
    lblGlobalInfoLabel->setStyleSheet ( ".QLabel { background: red; }" );
    lblGlobalInfoLabel->hide();

    // prepare update check info label (invisible by default)
    lblUpdateCheck->setOpenExternalLinks ( true ); // enables opening a web browser if one clicks on a html link
    lblUpdateCheck->setText ( "<font color=\"red\"><b>" + APP_UPGRADE_AVAILABLE_MSG_TEXT.arg ( APP_NAME ).arg ( VERSION ) + "</b></font>" );
    lblUpdateCheck->hide();
}

void CClientDlg::makeConnections()
{
    // TODO:
    // -- lots of these will change - settings will directly update and clientdialog will get emit from that
    //    and in other places the instances will manage for themselves
    //
    // UI Connections ----------------------------------------------------------
    // client menu extras - ideally, ClientSettingsDlg would do this wiring for itself (through the client instance)
    QObject::connect ( this, &CClientDlg::SendTabChange, &ClientSettingsDlg, &CClientSettingsDlg::OnClientMakeTabChange );

    QObject::connect ( sldAudioReverb, &QSlider::valueChanged, this, &CClientDlg::OnAudioReverbValueChanged );

    QObject::connect ( rbtReverbSelL, &QRadioButton::clicked, this, &CClientDlg::OnReverbSelLClicked );

    QObject::connect ( rbtReverbSelR, &QRadioButton::clicked, this, &CClientDlg::OnReverbSelRClicked );

    QObject::connect ( chbLocalMute, &QCheckBox::stateChanged, this, &CClientDlg::OnLocalMuteStateChanged );

    QObject::connect ( chbSettings, &QCheckBox::stateChanged, this, &CClientDlg::OnSettingsStateChanged );

    QObject::connect ( chbChat, &QCheckBox::stateChanged, this, &CClientDlg::OnChatStateChanged );

    QObject::connect ( butConnect, &QPushButton::clicked, this, &CClientDlg::OnConnectDisconBut );

    // mixerboard to client
    QObject::connect ( MainMixerBoard, &CAudioMixerBoard::ChangeChanGain, this, &CClientDlg::OnChangeChanGain );

    QObject::connect ( MainMixerBoard, &CAudioMixerBoard::ChangeChanPan, this, &CClientDlg::OnChangeChanPan );

    // chat to client
    QObject::connect ( &ChatDlg, &CChatDlg::NewLocalInputText, this, &CClientDlg::OnNewLocalInputText );

    // settings to where they need to go
    QObject::connect ( &ClientSettingsDlg, &CClientSettingsDlg::GUIDesignChanged, this, &CClientDlg::OnGUIDesignChanged );

    QObject::connect ( &ClientSettingsDlg, &CClientSettingsDlg::MeterStyleChanged, this, &CClientDlg::OnMeterStyleChanged );

    QObject::connect ( &ClientSettingsDlg, &CClientSettingsDlg::AudioChannelsChanged, this, &CClientDlg::OnAudioChannelsChanged );

    QObject::connect ( &ClientSettingsDlg, &CClientSettingsDlg::CustomDirectoriesChanged, &ConnectDlg, &CConnectDlg::OnCustomDirectoriesChanged );

    QObject::connect ( &ClientSettingsDlg, &CClientSettingsDlg::NumMixerPanelRowsChanged, this, &CClientDlg::OnNumMixerPanelRowsChanged );

    // connection setup window
    QObject::connect ( &ConnectDlg, &CConnectDlg::ReqServerListQuery, this, &CClientDlg::OnReqServerListQuery );

    // note that this connection must be a queued connection, otherwise the server list ping
    // times are not accurate and the client list may not be retrieved for all servers listed
    // (it seems the sendto() function needs to be called from different threads to fire the
    // packet immediately and do not collect packets before transmitting)
    QObject::connect ( &ConnectDlg, &CConnectDlg::CreateCLServerListPingMes, this, &CClientDlg::OnCreateCLServerListPingMes, Qt::QueuedConnection );

    QObject::connect ( &ConnectDlg, &CConnectDlg::CreateCLServerListReqVerAndOSMes, this, &CClientDlg::OnCreateCLServerListReqVerAndOSMes );

    QObject::connect ( &ConnectDlg,
                       &CConnectDlg::CreateCLServerListReqConnClientsListMes,
                       this,
                       &CClientDlg::OnCreateCLServerListReqConnClientsListMes );

    QObject::connect ( &ConnectDlg, &CConnectDlg::accepted, this, &CClientDlg::OnConnectDlgAccepted );

    // Timers ------------------------------------------------------------------
    QObject::connect ( &TimerSigMet, &QTimer::timeout, this, &CClientDlg::OnTimerSigMet );

    QObject::connect ( &TimerBuffersLED, &QTimer::timeout, this, &CClientDlg::OnTimerBuffersLED );

    QObject::connect ( &TimerStatus, &QTimer::timeout, this, &CClientDlg::OnTimerStatus );

    QObject::connect ( &TimerPing, &QTimer::timeout, this, &CClientDlg::OnTimerPing );

    QObject::connect ( &TimerDisplaySoundcardPopup, &QTimer::timeout, this, &CClientDlg::OnTimerCheckAudioDeviceOk );

    QObject::connect ( &TimerFeedbackDetectionPeriod, &QTimer::timeout, this, &CClientDlg::OnFeedbackDetectionExpired );

    // From client -------------------------------------------------------------
    QObject::connect ( &Client, &CClient::ConClientListMesReceived, this, &CClientDlg::OnConClientListMesReceived );

    QObject::connect ( &Client, &CClient::Disconnected, this, &CClientDlg::OnDisconnected );

    QObject::connect ( &Client, &CClient::ChatTextReceived, this, &CClientDlg::OnChatTextReceived );

    QObject::connect ( &Client, &CClient::ClientIDReceived, this, &CClientDlg::OnClientIDReceived );

    QObject::connect ( &Client, &CClient::MuteStateHasChangedReceived, this, &CClientDlg::OnMuteStateHasChangedReceived );

    QObject::connect ( &Client, &CClient::RecorderStateReceived, this, &CClientDlg::OnRecorderStateReceived );

    // This connection is a special case. On receiving a licence required message via the
    // protocol, a modal licence dialog is opened. Since this blocks the thread, we need
    // a queued connection to make sure the core protocol mechanism is not blocked, too.
    qRegisterMetaType<ELicenceType> ( "ELicenceType" );
    QObject::connect ( &Client, &CClient::LicenceRequired, this, &CClientDlg::OnLicenceRequired, Qt::QueuedConnection );

    QObject::connect ( &Client, &CClient::PingTimeReceived, this, &CClientDlg::OnPingTimeResult );

    QObject::connect ( &Client, &CClient::CLServerListReceived, this, &CClientDlg::OnCLServerListReceived );

    QObject::connect ( &Client, &CClient::CLRedServerListReceived, this, &CClientDlg::OnCLRedServerListReceived );

    QObject::connect ( &Client, &CClient::CLConnClientsListMesReceived, this, &CClientDlg::OnCLConnClientsListMesReceived );

    QObject::connect ( &Client, &CClient::CLPingTimeWithNumClientsReceived, this, &CClientDlg::OnCLPingTimeWithNumClientsReceived );

    QObject::connect ( &Client, &CClient::ControllerInFaderLevel, this, &CClientDlg::OnControllerInFaderLevel );

    QObject::connect ( &Client, &CClient::ControllerInPanValue, this, &CClientDlg::OnControllerInPanValue );

    QObject::connect ( &Client, &CClient::ControllerInFaderIsSolo, this, &CClientDlg::OnControllerInFaderIsSolo );

    QObject::connect ( &Client, &CClient::ControllerInFaderIsMute, this, &CClientDlg::OnControllerInFaderIsMute );

    QObject::connect ( &Client, &CClient::CLChannelLevelListReceived, this, &CClientDlg::OnCLChannelLevelListReceived );

    QObject::connect ( &Client, &CClient::VersionAndOSReceived, this, &CClientDlg::OnVersionAndOSReceived );

    QObject::connect ( &Client, &CClient::CLVersionAndOSReceived, this, &CClientDlg::OnCLVersionAndOSReceived );

    QObject::connect ( &Client, &CClient::SoundDeviceChanged, this, &CClientDlg::OnSoundDeviceChanged );
}

void CClientDlg::startTimers()
{
    // TODO these should not be in the UI!
    // setup timers
    TimerDisplaySoundcardPopup.setSingleShot ( true ); // only check once after connection...
    TimerFeedbackDetectionPeriod.setSingleShot ( true );

    // Initializations which have to be done after the signals are connected ---
    // start timer for status bar
    TimerStatus.start ( LED_BAR_UPDATE_TIME_MS );
}

void CClientDlg::restoreWindowGeometry()
{
    // Window positions --------------------------------------------------------
    // main window
    if ( !Client.GetMainWindowGeometry().isEmpty() && !Client.GetMainWindowGeometry().isNull() )
    {
        restoreGeometry ( Client.GetMainWindowGeometry() );
    }

    // settings window
    if ( !Client.GetSettingsWindowGeometry().isEmpty() && !Client.GetSettingsWindowGeometry().isNull() )
    {
        ClientSettingsDlg.restoreGeometry ( Client.GetSettingsWindowGeometry() );
    }

    if ( Client.IsSettingsWindowVisible() )
    {
        ShowGeneralSettings ( Client.GetSettingsTab() );
    }

    // chat window
    if ( !Client.GetChatWindowGeometry().isEmpty() && !Client.GetChatWindowGeometry().isNull() )
    {
        ChatDlg.restoreGeometry ( Client.GetChatWindowGeometry() );
    }

    if ( Client.IsChatWindowVisible() )
    {
        ShowChatWindow();
    }

    // connect window
    ConnectDlg.SetShowAllMusicians ( Client.IsShowAllMusicians() );

    if ( !Client.GetConnectWindowGeometry().isEmpty() && !Client.GetConnectWindowGeometry().isNull() )
    {
        ConnectDlg.restoreGeometry ( Client.GetConnectWindowGeometry() );
    }

    if ( Client.IsConnectWindowVisible() )
    {
        ShowConnectionSetupDialog();
    }
}

void CClientDlg::closeEvent ( QCloseEvent* Event )
{
    // make sure all current fader settings are applied to the settings struct
    MainMixerBoard->StoreAllFaderSettings();

    // ClientSettingsDlg doesn't expose selected tab and the setting is not used

    Client.SetShowAllMusicians ( ConnectDlg.GetShowAllMusicians() );

    Client.SetChannelSortType ( MainMixerBoard->GetFaderSorting() );
    Client.SetNumMixerPanelRows ( MainMixerBoard->GetNumMixerPanelRows() );

    // store window positions
    Client.SetMainWindowGeometry ( saveGeometry() );
    Client.SetSettingsWindowGeometry ( ClientSettingsDlg.saveGeometry() );
    Client.SetChatWindowGeometry ( ChatDlg.saveGeometry() );
    Client.SetConnectWindowGeometry ( ConnectDlg.saveGeometry() );

    // store child window visiblity
    Client.SetSettingsWindowVisible ( ClientSettingsDlg.isVisible() );
    Client.SetChatWindowVisible ( ChatDlg.isVisible() );
    Client.SetConnectWindowVisible ( ConnectDlg.isVisible() );

    // close child windows
    ClientSettingsDlg.close();
    ChatDlg.close();
    ConnectDlg.close();
    AnalyzerConsole.close();

    // if connected, terminate connection -- client should do this for itself?
    if ( Client.IsRunning() )
    {
        Client.Stop();
    }

    // default implementation of this event handler routine
    Event->accept();
}

void CClientDlg::ManageDragNDrop ( QDropEvent* Event, const bool bCheckAccept )
{
    // we only want to use drag'n'drop with file URLs
    QListIterator<QUrl> UrlIterator ( Event->mimeData()->urls() );

    while ( UrlIterator.hasNext() )
    {
        const QString strFileNameWithPath = UrlIterator.next().toLocalFile();

        // check all given URLs and if any has the correct suffix
        if ( !strFileNameWithPath.isEmpty() && ( QFileInfo ( strFileNameWithPath ).suffix() == MIX_SETTINGS_FILE_SUFFIX ) )
        {
            if ( bCheckAccept )
            {
                // only accept drops of supports file types
                Event->acceptProposedAction();
            }
            else
            {
                // load the first valid settings file and leave the loop
                Client.LoadFaderSettings ( strFileNameWithPath );
                MainMixerBoard->LoadAllFaderSettings();
                break;
            }
        }
    }
}

void CClientDlg::UpdateRevSelection()
{
    if ( Client.GetAudioChannels() == CC_STEREO )
    {
        // for stereo make channel selection invisible since
        // reverberation effect is always applied to both channels
        rbtReverbSelL->setVisible ( false );
        rbtReverbSelR->setVisible ( false );
    }
    else
    {
        // make radio buttons visible
        rbtReverbSelL->setVisible ( true );
        rbtReverbSelR->setVisible ( true );

        // update value
        if ( Client.IsReverbOnLeftChan() )
        {
            rbtReverbSelL->setChecked ( true );
        }
        else
        {
            rbtReverbSelR->setChecked ( true );
        }
    }

    // update visibility of the pan controls in the audio mixer board (pan is not supported for mono)
    MainMixerBoard->SetDisplayPans ( Client.GetAudioChannels() != CC_MONO );
}

void CClientDlg::OnConnectDlgAccepted()
{
    // We had an issue that the accepted signal was emit twice if a list item was double
    // clicked in the connect dialog. To avoid this we introduced a flag which makes sure
    // we process the accepted signal only once after the dialog was initially shown.
    if ( bConnectDlgWasShown )
    {
        // get the address from the connect dialog
        QString strSelectedAddress = ConnectDlg.GetSelectedAddress();

        // only store new host address in our data base if the address is
        // not empty and it was not a server list item (only the addresses
        // typed in manually are stored by definition)
        if ( !strSelectedAddress.isEmpty() && !ConnectDlg.GetServerListItemWasChosen() )
        {
            // store new address at the top of the list, if the list was already
            // full, the last element is thrown out
            Client.StoreServerAddress ( strSelectedAddress );
        }

        // get name to be set in audio mixer group box title
        QString strMixerBoardLabel;

        if ( ConnectDlg.GetServerListItemWasChosen() )
        {
            // in case a server in the server list was chosen,
            // display the server name of the server list
            strMixerBoardLabel = ConnectDlg.GetSelectedServerName();
        }
        else
        {
            // an item of the server address combo box was chosen,
            // just show the address string as it was entered by the
            // user
            strMixerBoardLabel = strSelectedAddress;

            // special case: if the address is empty, we substitute the default
            // directory address so that a user who just pressed the connect
            // button without selecting an item in the table or manually entered an
            // address gets a successful connection
            if ( strSelectedAddress.isEmpty() )
            {
                strSelectedAddress = DEFAULT_SERVER_ADDRESS;
                strMixerBoardLabel = tr ( "%1 Directory" ).arg ( DirectoryTypeToString ( AT_DEFAULT ) );
            }
        }

        // first check if we are already connected, if this is the case we have to
        // disconnect the old server first
        if ( Client.IsRunning() )
        {
            Disconnect();
        }

        // initiate connection
        Connect ( strSelectedAddress, strMixerBoardLabel );

        // reset flag
        bConnectDlgWasShown = false;
    }
}

void CClientDlg::OnConnectDisconBut()
{
    // the connect/disconnect button implements a toggle functionality
    if ( Client.IsRunning() )
    {
        Disconnect();
        UpdateMixerBoardDeco ( RS_UNDEFINED, Client.GetGUIDesign() );
    }
    else
    {
        ShowConnectionSetupDialog();
    }
}

void CClientDlg::OnClearAllStoredSoloMuteSettings()
{
    // if we are in an active connection, we first have to store all fader settings in
    // the settings struct, clear the solo and mute states and then apply the settings again
    MainMixerBoard->StoreAllFaderSettings();
    Client.ClearStoredFaderIsSolo();
    Client.ClearStoredFaderIsMute();
    MainMixerBoard->LoadAllFaderSettings();
}

void CClientDlg::OnLoadChannelSetup()
{
    QString strFileName = QFileDialog::getOpenFileName ( this, tr ( "Select Channel Setup File" ), "", QString ( "*." ) + MIX_SETTINGS_FILE_SUFFIX );

    if ( !strFileName.isEmpty() )
    {
        // first update the settings struct and then update the mixer panel
        Client.LoadFaderSettings ( strFileName );
        MainMixerBoard->LoadAllFaderSettings();
    }
}

void CClientDlg::OnSaveChannelSetup()
{
    QString strFileName = QFileDialog::getSaveFileName ( this, tr ( "Select Channel Setup File" ), "", QString ( "*." ) + MIX_SETTINGS_FILE_SUFFIX );

    if ( !strFileName.isEmpty() )
    {
        // first store all current fader settings (in case we are in an active connection
        // right now) and then save the information in the settings struct in the file
        MainMixerBoard->StoreAllFaderSettings();
        Client.SaveFaderSettings ( strFileName );
    }
}

void CClientDlg::OnVersionAndOSReceived ( COSUtil::EOpSystemType, QString strVersion )
{
    // check if Pan is supported by the server (minimum version is 3.5.4)
#if QT_VERSION >= QT_VERSION_CHECK( 5, 6, 0 )
    if ( QVersionNumber::compare ( QVersionNumber::fromString ( strVersion ), QVersionNumber ( 3, 5, 4 ) ) >= 0 )
    {
        MainMixerBoard->SetPanIsSupported();
    }
#endif
}

void CClientDlg::OnCLVersionAndOSReceived ( CHostAddress, COSUtil::EOpSystemType, QString strVersion )
{
    // update check
#if ( QT_VERSION >= QT_VERSION_CHECK( 5, 6, 0 ) ) && !defined( DISABLE_VERSION_CHECK )
    int            mySuffixIndex;
    QVersionNumber myVersion = QVersionNumber::fromString ( VERSION, &mySuffixIndex );

    int            serverSuffixIndex;
    QVersionNumber serverVersion = QVersionNumber::fromString ( strVersion, &serverSuffixIndex );

    // only compare if the server version has no suffix (such as dev or beta)
    if ( strVersion.size() == serverSuffixIndex && QVersionNumber::compare ( serverVersion, myVersion ) > 0 )
    {
        // show the label and hide it after one minute again
        lblUpdateCheck->show();
        QTimer::singleShot ( 60000, [this]() { lblUpdateCheck->hide(); } );
    }
#endif
}

void CClientDlg::OnChatTextReceived ( QString strChatText )
{
    ChatDlg.AddChatText ( strChatText );

    // Open chat dialog. If a server welcome message is received, we force
    // the dialog to be upfront in case a licence text is shown. For all
    // other new chat texts we do not want to force the dialog to be upfront
    // always when a new message arrives since this is annoying.
    ShowChatWindow ( ( strChatText.indexOf ( WELCOME_MESSAGE_PREFIX ) == 0 ) );

    UpdateDisplay();
}

void CClientDlg::OnLicenceRequired ( ELicenceType eLicenceType )
{
    // right now only the creative common licence is supported
    if ( eLicenceType == LT_CREATIVECOMMONS )
    {
        CLicenceDlg LicenceDlg;

        // mute the client output stream
        Client.SetMuteOutStream ( true );

        // Open the licence dialog and check if the licence was accepted. In
        // case the dialog is just closed or the decline button was pressed,
        // disconnect from that server.
        if ( !LicenceDlg.exec() )
        {
            Disconnect();
        }

        // unmute the client output stream if local mute button is not pressed
        if ( chbLocalMute->checkState() == Qt::Unchecked )
        {
            Client.SetMuteOutStream ( false );
        }
    }
}

void CClientDlg::OnConClientListMesReceived ( CVector<CChannelInfo> vecChanInfo )
{
    // update mixer board with the additional client infos
    MainMixerBoard->ApplyNewConClientList ( vecChanInfo );

    // Update window title with number of connected clients
    SetMyWindowTitle ( vecChanInfo.Size() );
}

void CClientDlg::OnNumClientsChanged ( int iNewNumClients )
{
    // update window title
    SetMyWindowTitle ( iNewNumClients );
}

void CClientDlg::OnOpenAudioNetSettings() { ShowGeneralSettings ( SETTING_TAB_AUDIONET ); }

void CClientDlg::OnOpenAdvancedSettings() { ShowGeneralSettings ( SETTING_TAB_ADVANCED ); }

void CClientDlg::OnOpenUserProfileSettings() { ShowGeneralSettings ( SETTING_TAB_USER ); }

void CClientDlg::SetMyWindowTitle ( const int iNumClients )
{
    // set the window title (and therefore also the task bar icon text of the OS)
    // according to the following specification (#559):
    // <ServerName> - <N> users - Jamulus
    QString    strWinTitle;
    const bool bClientNameIsUsed = !Client.GetClientName().isEmpty();

    if ( bClientNameIsUsed )
    {
        // if --clientname is used, the APP_NAME must be the very first word in
        // the title, otherwise some user scripts do not work anymore, see #789
        strWinTitle += QString ( APP_NAME ) + " - " + Client.GetClientName() + " ";
    }

    if ( iNumClients == 0 )
    {
        // only application name
        if ( !bClientNameIsUsed ) // if --clientname is used, the APP_NAME is the first word in title
        {
            strWinTitle += QString ( APP_NAME );
        }
    }
    else
    {
        strWinTitle += MainMixerBoard->GetServerName();

        if ( iNumClients == 1 )
        {
            strWinTitle += " - 1 " + tr ( "user" );
        }
        else if ( iNumClients > 1 )
        {
            strWinTitle += " - " + QString::number ( iNumClients ) + " " + tr ( "users" );
        }

        if ( !bClientNameIsUsed ) // if --clientname is used, the APP_NAME is the first word in title
        {
            strWinTitle += " - " + QString ( APP_NAME );
        }
    }

    setWindowTitle ( strWinTitle );

#if defined( Q_OS_MACX )
    // for MacOS only we show the number of connected clients as a
    // badge label text if more than one user is connected
    if ( iNumClients > 1 )
    {
        // show the number of connected clients
        SetMacBadgeLabelText ( QString ( "%1" ).arg ( iNumClients ) );
    }
    else
    {
        // clear the text (apply an empty string)
        SetMacBadgeLabelText ( "" );
    }
#endif
}

void CClientDlg::ShowConnectionSetupDialog()
{
    // show connect dialog
    bConnectDlgWasShown = true;
    ConnectDlg.setWindowTitle ( MakeClientNameTitle ( tr ( "Connect" ), Client.GetClientName() ) );
    ConnectDlg.show();

    // make sure dialog is upfront and has focus
    ConnectDlg.raise();
    ConnectDlg.activateWindow();
}

void CClientDlg::ShowGeneralSettings ( int iTab )
{
    // open general settings dialog
    emit SendTabChange ( iTab );
    ClientSettingsDlg.setWindowTitle ( MakeClientNameTitle ( tr ( "Settings" ), Client.GetClientName() ) );
    ClientSettingsDlg.show();

    // make sure dialog is upfront and has focus
    ClientSettingsDlg.raise();
    ClientSettingsDlg.activateWindow();
}

void CClientDlg::ShowChatWindow ( const bool bForceRaise )
{
    ChatDlg.setWindowTitle ( MakeClientNameTitle ( tr ( "Chat" ), Client.GetClientName() ) );
    ChatDlg.show();

    if ( bForceRaise )
    {
        // make sure dialog is upfront and has focus
        ChatDlg.showNormal();
        ChatDlg.raise();
        ChatDlg.activateWindow();
    }

    UpdateDisplay();
}

void CClientDlg::ShowAnalyzerConsole()
{
    // open analyzer console dialog
    AnalyzerConsole.show();

    // make sure dialog is upfront and has focus
    AnalyzerConsole.raise();
    AnalyzerConsole.activateWindow();
}

void CClientDlg::OnSettingsStateChanged ( int value )
{
    if ( value == Qt::Checked )
    {
        ShowGeneralSettings ( SETTING_TAB_AUDIONET );
    }
    else
    {
        ClientSettingsDlg.hide();
    }
}

void CClientDlg::OnChatStateChanged ( int value )
{
    if ( value == Qt::Checked )
    {
        ShowChatWindow();
    }
    else
    {
        ChatDlg.hide();
    }
}

void CClientDlg::OnLocalMuteStateChanged ( int value )
{
    Client.SetMuteOutStream ( value == Qt::Checked );

    // show/hide info label
    if ( value == Qt::Checked )
    {
        lblGlobalInfoLabel->show();
    }
    else
    {
        lblGlobalInfoLabel->hide();
    }
}

/***
 * TODO:
 * CClient should monitor for feedback and emit a signal when it is detected
 * This handler should then just do the UI part (everything except the "if")
\***/
void CClientDlg::OnTimerSigMet()
{
    // show current level
    lbrInputLevelL->SetValue ( Client.GetLevelForMeterdBLeft() );
    lbrInputLevelR->SetValue ( Client.GetLevelForMeterdBRight() );

    if ( bDetectingFeedback &&
         ( Client.GetLevelForMeterdBLeft() > NUM_STEPS_LED_BAR - 0.5 || Client.GetLevelForMeterdBRight() > NUM_STEPS_LED_BAR - 0.5 ) )
    {
        // mute locally and mute channel
        chbLocalMute->setCheckState ( Qt::Checked );
        MainMixerBoard->MuteMyChannel();

        // show message box about feedback issue
        QCheckBox* chb = new QCheckBox ( tr ( "Enable feedback detection" ) );
        chb->setCheckState ( Client.IsEnableFeedbackDetection() ? Qt::Checked : Qt::Unchecked );
        QMessageBox msgbox;
        msgbox.setText ( tr ( "Audio feedback or loud signal detected.\n\n"
                              "We muted your channel and activated 'Mute Myself'. Please solve "
                              "the feedback issue first and unmute yourself afterwards." ) );
        msgbox.setIcon ( QMessageBox::Icon::Warning );
        msgbox.addButton ( QMessageBox::Ok );
        msgbox.setDefaultButton ( QMessageBox::Ok );
        msgbox.setCheckBox ( chb );

        QObject::connect ( chb, &QCheckBox::stateChanged, &ClientSettingsDlg, &CClientSettingsDlg::OnClientFeedbackDetectionChanged );

        msgbox.exec();
    }
}

void CClientDlg::OnTimerBuffersLED()
{
    CMultiColorLED::ELightColor eCurStatus;

    // get and reset current buffer state and set LED from that flag
    if ( Client.GetAndResetbJitterBufferOKFlag() )
    {
        eCurStatus = CMultiColorLED::RL_GREEN;
    }
    else
    {
        eCurStatus = CMultiColorLED::RL_RED;
    }

    // update the buffer LED and the general settings dialog, too
    ledBuffers->SetLight ( eCurStatus );
}

void CClientDlg::OnTimerPing()
{
    // send ping message to the server
    Client.CreateCLPingMes();
}

void CClientDlg::OnPingTimeResult ( int iPingTime )
{
    // calculate overall delay
    const int iOverallDelayMs = Client.EstimatedOverallDelay ( iPingTime );

    // color definition: <= 43 ms green, <= 68 ms yellow, otherwise red
    CMultiColorLED::ELightColor eOverallDelayLEDColor;

    if ( iOverallDelayMs <= 43 )
    {
        eOverallDelayLEDColor = CMultiColorLED::RL_GREEN;
    }
    else
    {
        if ( iOverallDelayMs <= 68 )
        {
            eOverallDelayLEDColor = CMultiColorLED::RL_YELLOW;
        }
        else
        {
            eOverallDelayLEDColor = CMultiColorLED::RL_RED;
        }
    }

    // only update delay information on settings dialog if it is visible to
    // avoid CPU load on working thread if not necessary
    if ( ClientSettingsDlg.isVisible() )
    {
        // set ping time result to general settings dialog
        ClientSettingsDlg.UpdateUploadRate();
    }
    SetPingTime ( iPingTime, iOverallDelayMs, eOverallDelayLEDColor );

    // update delay LED on the main window
    ledDelay->SetLight ( eOverallDelayLEDColor );
}

void CClientDlg::OnTimerCheckAudioDeviceOk()
{
    // check if the audio device entered the audio callback after a pre-defined
    // timeout to check if a valid device is selected and if we do not have
    // fundamental settings errors (in which case the GUI would only show that
    // it is trying to connect the server which does not help to solve the problem (#129))
    if ( !Client.IsCallbackEntered() )
    {
        QMessageBox::warning ( this,
                               APP_NAME,
                               tr ( "Your sound card is not working correctly. "
                                    "Please open the settings dialog and check the device selection and the driver settings." ) );
    }
}

void CClientDlg::OnFeedbackDetectionExpired() { bDetectingFeedback = false; }

void CClientDlg::OnSoundDeviceChanged ( QString strError )
{
    if ( !strError.isEmpty() )
    {
        // the sound device setup has a problem, disconnect any active connection
        if ( Client.IsRunning() )
        {
            Disconnect();
        }

        // show the error message of the device setup
        QMessageBox::critical ( this, APP_NAME, strError, tr ( "Ok" ), nullptr );
    }

    // if the check audio device timer is running, it must be restarted on a device change
    if ( TimerDisplaySoundcardPopup.isActive() )
    {
        TimerDisplaySoundcardPopup.start ( CHECK_AUDIO_DEV_OK_TIME_MS );
    }

    if ( Client.IsEnableFeedbackDetection() && TimerFeedbackDetectionPeriod.isActive() ) // why only while it's active?
    {
        TimerFeedbackDetectionPeriod.start ( DETECT_FEEDBACK_TIME_MS );
        bDetectingFeedback = true;
    }

    // update the settings dialog
    ClientSettingsDlg.UpdateSoundDeviceChannelSelectionFrame();
}

void CClientDlg::OnCLPingTimeWithNumClientsReceived ( CHostAddress InetAddr, int iPingTime, int iNumClients )
{
    // update connection dialog server list
    ConnectDlg.SetPingTimeAndNumClientsResult ( InetAddr, iPingTime, iNumClients );
}

void CClientDlg::Connect ( const QString& strSelectedAddress, const QString& strMixerBoardLabel )
{
    // set address and check if address is valid
    if ( Client.SetServerAddr ( strSelectedAddress ) )
    {
        // try to start client, if error occurred, do not go in
        // running state but show error message
        try
        {
            if ( !Client.IsRunning() )
            {
                Client.Start();
            }
        }

        catch ( const CGenErr& generr )
        {
            // show error message and return the function
            QMessageBox::critical ( this, APP_NAME, generr.GetErrorText(), "Close", nullptr );
            return;
        }

        SetConnectedGUI ( strMixerBoardLabel );
    }
}

void CClientDlg::SetConnectedGUI ( const QString& strMixerBoardLabel )
{
    // hide label connect to server
    lblConnectToServer->hide();
    lbrInputLevelL->setEnabled ( true );
    lbrInputLevelR->setEnabled ( true );

    // change connect button text to "disconnect"
    butConnect->setText ( tr ( "&Disconnect" ) );

    // set server name in audio mixer group box title
    MainMixerBoard->SetServerName ( strMixerBoardLabel );

    // start timer for level meter bar and ping time measurement
    TimerSigMet.start ( LEVELMETER_UPDATE_TIME_MS );
    TimerBuffersLED.start ( BUFFER_LED_UPDATE_TIME_MS );
    TimerPing.start ( PING_UPDATE_TIME_MS );
    TimerDisplaySoundcardPopup.start ( CHECK_AUDIO_DEV_OK_TIME_MS ); // is single shot timer

    // audio feedback detection
    if ( Client.IsEnableFeedbackDetection() )
    {
        TimerFeedbackDetectionPeriod.start ( DETECT_FEEDBACK_TIME_MS ); // single shot timer
        bDetectingFeedback = true;
    }
}

void CClientDlg::Disconnect()
{
    // only stop client if currently running, in case we received
    // the stopped message, the client is already stopped but the
    // connect/disconnect button and other GUI controls must be
    // updated
    if ( Client.IsRunning() )
    {
        Client.Stop();
    }

    // change connect button text to "connect"
    butConnect->setText ( tr ( "C&onnect" ) );

    // reset server name in audio mixer group box title
    MainMixerBoard->SetServerName ( "" );

    // stop timer for level meter bars and reset them
    TimerSigMet.stop();
    lbrInputLevelL->setEnabled ( false );
    lbrInputLevelR->setEnabled ( false );
    lbrInputLevelL->SetValue ( 0 );
    lbrInputLevelR->SetValue ( 0 );

    // show connect to server message
    lblConnectToServer->show();

    // stop other timers
    TimerBuffersLED.stop();
    TimerPing.stop();
    TimerDisplaySoundcardPopup.stop();
    TimerFeedbackDetectionPeriod.stop();
    bDetectingFeedback = false;

    //### TODO: BEGIN ###//
    // is this still required???
    // immediately update status bar
    OnTimerStatus();
    //### TODO: END ###//

    // reset LEDs
    ledBuffers->Reset();
    ledDelay->Reset();

    // clear text labels with client parameters
    lblPingVal->setText ( "---" );
    lblPingUnit->setText ( "" );
    lblDelayVal->setText ( "---" );
    lblDelayUnit->setText ( "" );

    // clear mixer board (remove all faders)
    MainMixerBoard->HideAll();
}

void CClientDlg::UpdateDisplay()
{
    // update settings/chat buttons (do not fire signals since it is an update)
    if ( chbSettings->isChecked() && !ClientSettingsDlg.isVisible() )
    {
        chbSettings->blockSignals ( true );
        chbSettings->setChecked ( false );
        chbSettings->blockSignals ( false );
    }
    if ( !chbSettings->isChecked() && ClientSettingsDlg.isVisible() )
    {
        chbSettings->blockSignals ( true );
        chbSettings->setChecked ( true );
        chbSettings->blockSignals ( false );
    }

    if ( chbChat->isChecked() && !ChatDlg.isVisible() )
    {
        chbChat->blockSignals ( true );
        chbChat->setChecked ( false );
        chbChat->blockSignals ( false );
    }
    if ( !chbChat->isChecked() && ChatDlg.isVisible() )
    {
        chbChat->blockSignals ( true );
        chbChat->setChecked ( true );
        chbChat->blockSignals ( false );
    }
}

void CClientDlg::OnGUIDesignChanged ( const EGUIDesign value )
{
    Client.SetGUIDesign ( value );
    UpdateGUIDesign();

    // also apply GUI design to child GUI controls
    MainMixerBoard->SetGUIDesign ( value );
    UpdateMixerBoardDeco ( MainMixerBoard->GetRecorderState(), value );
}

void CClientDlg::UpdateGUIDesign()
{
    const EGUIDesign eNewDesign = Client.GetGUIDesign();

    // remove any styling from the mixer board - reapply after changing skin
    MainMixerBoard->setStyleSheet ( "" );

    // apply GUI design to current window
    switch ( eNewDesign )
    {
    case GD_ORIGINAL:
        backgroundFrame->setStyleSheet (
            "QFrame#backgroundFrame { border-image:  url(:/png/fader/res/mixerboardbackground.png) 34px 30px 40px 40px;"
            "                         border-top:    34px transparent;"
            "                         border-bottom: 40px transparent;"
            "                         border-left:   30px transparent;"
            "                         border-right:  40px transparent;"
            "                         padding:       -5px;"
            "                         margin:        -5px, -5px, 0px, 0px; }"
            "QLabel {                 color:          rgb(220, 220, 220);"
            "                         font:           bold; }"
            "QRadioButton {           color:          rgb(220, 220, 220);"
            "                         font:           bold; }"
            "QScrollArea {            background:     transparent; }"
            ".QWidget {               background:     transparent; }" // note: matches instances of QWidget, but not of its subclasses
            "QGroupBox {              background:     transparent; }"
            "QGroupBox::title {       color:          rgb(220, 220, 220); }"
            "QCheckBox::indicator {   width:          38px;"
            "                         height:         21px; }"
            "QCheckBox::indicator:unchecked {"
            "                         image:          url(:/png/fader/res/ledbuttonnotpressed.png); }"
            "QCheckBox::indicator:checked {"
            "                         image:          url(:/png/fader/res/ledbuttonpressed.png); }"
            "QCheckBox {              color:          rgb(220, 220, 220);"
            "                         font:           bold; }" );
#ifdef _WIN32
        // Workaround QT-Windows problem: This should not be necessary since in the
        // background frame the style sheet for QRadioButton was already set. But it
        // seems that it is only applied if the style was set to default and then back
        // to GD_ORIGINAL. This seems to be a QT related issue...
        rbtReverbSelL->setStyleSheet ( "color: rgb(220, 220, 220);"
                                       "font:  bold;" );
        rbtReverbSelR->setStyleSheet ( "color: rgb(220, 220, 220);"
                                       "font:  bold;" );
#endif

        ledBuffers->SetType ( CMultiColorLED::MT_LED );
        ledDelay->SetType ( CMultiColorLED::MT_LED );
        break;

    default:
        // reset style sheet and set original parameters
        backgroundFrame->setStyleSheet ( "" );

#ifdef _WIN32
        // Workaround QT-Windows problem: See above description
        rbtReverbSelL->setStyleSheet ( "" );
        rbtReverbSelR->setStyleSheet ( "" );
#endif

        ledBuffers->SetType ( CMultiColorLED::MT_INDICATOR );
        ledDelay->SetType ( CMultiColorLED::MT_INDICATOR );
        break;
    }
}

void CClientDlg::OnMeterStyleChanged ( const EMeterStyle eNewMeterStyle )
{
    Client.SetMeterStyle ( eNewMeterStyle );
    UpdateMeterStyle();

    // also apply MeterStyle to child GUI controls
    MainMixerBoard->SetMeterStyle ( eNewMeterStyle );
}

void CClientDlg::UpdateMeterStyle()
{
    const EMeterStyle eNewMeterStyle = Client.GetMeterStyle();

    // apply MeterStyle to current window
    switch ( eNewMeterStyle )
    {
    case MT_LED_STRIPE:
        lbrInputLevelL->SetLevelMeterType ( CLevelMeter::MT_LED_STRIPE );
        lbrInputLevelR->SetLevelMeterType ( CLevelMeter::MT_LED_STRIPE );
        break;

    case MT_LED_ROUND_BIG:
        lbrInputLevelL->SetLevelMeterType ( CLevelMeter::MT_LED_ROUND_BIG );
        lbrInputLevelR->SetLevelMeterType ( CLevelMeter::MT_LED_ROUND_BIG );
        break;

    case MT_BAR_WIDE:
        lbrInputLevelL->SetLevelMeterType ( CLevelMeter::MT_BAR_WIDE );
        lbrInputLevelR->SetLevelMeterType ( CLevelMeter::MT_BAR_WIDE );
        break;

    case MT_BAR_NARROW:
        lbrInputLevelL->SetLevelMeterType ( CLevelMeter::MT_BAR_WIDE );
        lbrInputLevelR->SetLevelMeterType ( CLevelMeter::MT_BAR_WIDE );
        break;

    case MT_LED_ROUND_SMALL:
        lbrInputLevelL->SetLevelMeterType ( CLevelMeter::MT_LED_ROUND_BIG );
        lbrInputLevelR->SetLevelMeterType ( CLevelMeter::MT_LED_ROUND_BIG );
        break;
    }
}

void CClientDlg::OnRecorderStateReceived ( const ERecorderState newRecorderState )
{
    MainMixerBoard->SetRecorderState ( newRecorderState );
    UpdateMixerBoardDeco ( newRecorderState, Client.GetGUIDesign() );
}

void CClientDlg::UpdateMixerBoardDeco ( const ERecorderState newRecorderState, const EGUIDesign eNewDesign )
{
    // return if no change
    if ( ( newRecorderState == eLastRecorderState ) && ( eNewDesign == eLastDesign ) )
    {
        return;
    }

    eLastRecorderState = newRecorderState;
    eLastDesign        = eNewDesign;

    // set base style
    QString sTitleStyle = "QGroupBox::title { subcontrol-origin: margin;"
                          "                   subcontrol-position: left top;"
                          "                   left: 7px;";

    if ( newRecorderState == RS_RECORDING )
    {
        sTitleStyle += "color: rgb(255,255,255);"
                       "background-color: rgb(255,0,0); }";
    }
    else
    {
        if ( eNewDesign == GD_ORIGINAL )
        {
            // no need to set the background color for dark mode in fancy skin, as the background is already dark.
            sTitleStyle += "color: rgb(220,220,220); }";
        }
        else
        {
            if ( palette().color ( QPalette::Window ) == QColor::fromRgbF ( 0.196078, 0.196078, 0.196078, 1 ) )
            {
                // Dark mode on macOS/Linux needs a light color

                sTitleStyle += "color: rgb(220,220,220); }";
            }
            else
            {
                sTitleStyle += "color: rgb(0,0,0); }";
            }
        }
    }

    MainMixerBoard->setStyleSheet ( sTitleStyle );
}

void CClientDlg::SetPingTime ( const int iPingTime, const int iOverallDelayMs, const CMultiColorLED::ELightColor eOverallDelayLEDColor )
{
    // apply values to GUI labels, take special care if ping time exceeds
    // a certain value
    if ( iPingTime > 500 )
    {
        const QString sErrorText = "<font color=\"red\"><b>&#62;500</b></font>";
        lblPingVal->setText ( sErrorText );
        lblDelayVal->setText ( sErrorText );
    }
    else
    {
        lblPingVal->setText ( QString().setNum ( iPingTime ) );
        lblDelayVal->setText ( QString().setNum ( iOverallDelayMs ) );
    }
    lblPingUnit->setText ( "ms" );
    lblDelayUnit->setText ( "ms" );

    // set current LED status
    ledDelay->SetLight ( eOverallDelayLEDColor );
}
