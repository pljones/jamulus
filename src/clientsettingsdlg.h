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

#include <QLabel>
#include <QString>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QWhatsThis>
#include <QTimer>
#include <QSlider>
#include <QRadioButton>
#include <QMenuBar>
#include <QLayout>
#include <QButtonGroup>
#include <QMessageBox>
#include "global.h"
#include "util.h"
#include "client.h"
#include "settings.h"
#include "multicolorled.h"
#include "ui_clientsettingsdlgbase.h"

/* Definitions ****************************************************************/
// update time for GUI controls
#define DISPLAY_UPDATE_TIME 1000 // ms

/* Classes ********************************************************************/
class CClientSettingsDlg : public CBaseDlg, private Ui_CClientSettingsDlgBase
{
    Q_OBJECT

public:
    CClientSettingsDlg ( CClient& client, QWidget* parent = nullptr );

    void UpdateUploadRate();
    void UpdateSoundDeviceChannelSelectionFrame();

signals:
    void GUIDesignChanged ( const EGUIDesign value );
    void MeterStyleChanged ( const EMeterStyle eNewMeterStyle );
    void AudioAlertsChanged();
    void AudioChannelsChanged();
    void CustomDirectoriesChanged();
    void NumMixerPanelRowsChanged ( int value );

public slots:
    void OnClientFeedbackDetectionChanged ( int value );
    void OnClientMakeTabChange ( int iTabIdx );

protected:
    void createUI();
    void makeConnections();

    void UpdateDisplay();
    void UpdateJitterBufferFrame();
    void UpdateSoundCardFrame();
    void UpdateDirectoryComboBox();
    void UpdateAudioFaderSlider();

    QString GenSndCrdBufferDelayString ( const int iFrameSize, const QString strAddText = "" );

    virtual void showEvent ( QShowEvent* );

    CClient&     Client;
    QTimer       TimerStatus;
    QButtonGroup SndCrdBufferDelayButtonGroup;

protected slots:
    void OnTimerStatus() { UpdateDisplay(); }
    void OnNetBufValueChanged ( int value );
    void OnNetBufServerValueChanged ( int value );
    void OnAutoJitBufStateChanged ( int value );
    void OnEnableOPUS64StateChanged ( int value );
    void OnFeedbackDetectionChanged ( int value );
    void OnCustomDirectoriesEditingFinished();
    void OnNewClientLevelEditingFinished() { Client.SetNewClientFaderLevel ( edtNewClientLevel->text().toInt() ); }
    void OnInputBoostChanged();
    void OnSndCrdBufferDelayButtonGroupClicked ( QAbstractButton* button );
    void OnSoundcardActivated ( int iSndDevIdx );
    void OnLInChanActivated ( int iChanIdx );
    void OnRInChanActivated ( int iChanIdx );
    void OnLOutChanActivated ( int iChanIdx );
    void OnROutChanActivated ( int iChanIdx );
    void OnAudioChannelsActivated ( int iChanIdx );
    void OnAudioQualityActivated ( int iQualityIdx );
    void OnGUIDesignActivated ( int iDesignIdx );
    void OnMeterStyleActivated ( int iMeterStyleIdx );
    void OnAudioAlertsChanged ( int value ) { Client.SetAudioAlertsActive ( value == Qt::Checked ); }
    void OnLanguageChanged ( QString strLanguage ) { Client.SetLanguage ( strLanguage ); }
    void OnAliasTextChanged ( const QString& strNewName );
    void OnInstrumentActivated ( int iCntryListItem );
    void OnCountryActivated ( int iCntryListItem );
    void OnCityTextChanged ( const QString& strNewName );
    void OnSkillActivated ( int iCntryListItem );
    void OnTabChanged();
    void OnAudioPanValueChanged ( int value );

#if defined( _WIN32 ) && !defined( WITH_JACK )
    // Only include this slot for Windows when JACK is NOT used
    void OnDriverSetupClicked();
#endif
};
