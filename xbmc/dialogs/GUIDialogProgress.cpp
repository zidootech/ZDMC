/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIDialogProgress.h"

#include "Application.h"
#include "guilib/GUIProgressControl.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/LocalizeStrings.h"
#include "guilib/guiinfo/GUIInfoLabels.h"
#include "threads/SingleLock.h"
#include "utils/Variant.h"
#include "utils/log.h"

CGUIDialogProgress::CGUIDialogProgress(void)
    : CGUIDialogBoxBase(WINDOW_DIALOG_PROGRESS, "DialogConfirm.xml")
{
  Reset();
}

CGUIDialogProgress::~CGUIDialogProgress(void) = default;

void CGUIDialogProgress::Reset()
{
  CSingleLock lock(m_section);
  m_iCurrent = 0;
  m_iMax = 0;
  m_percentage = 0;
  m_showProgress = true;
  m_bCanCancel = true;
  m_iChoice = CHOICE_NONE;
  m_supportedChoices = {};

  SetInvalid();
}

void CGUIDialogProgress::SetCanCancel(bool bCanCancel)
{
  CSingleLock lock(m_section);
  m_bCanCancel = bCanCancel;
  SetInvalid();
}

void CGUIDialogProgress::ShowChoice(int iChoice, const CVariant& label)
{
  if (iChoice >= 0 && iChoice < DIALOG_MAX_CHOICES)
  {
    m_supportedChoices[iChoice] = true;
    SetChoice(iChoice, label);
    SetInvalid();
  }
}

int CGUIDialogProgress::GetChoice() const
{
  return m_iChoice;
}

void CGUIDialogProgress::Open(const std::string &param /* = "" */)
{
  CLog::Log(LOGDEBUG, "DialogProgress::Open called %s", m_active ? "(already running)!" : "");

  {
    CSingleLock lock(CServiceBroker::GetWinSystem()->GetGfxContext());
    ShowProgressBar(true);
  }

  CGUIDialog::Open(false, param);

  while (m_active && IsAnimating(ANIM_TYPE_WINDOW_OPEN))
  {
    Progress();
    // we should have rendered at least once by now - if we haven't, then
    // we must be running from fullscreen video or similar where the
    // calling thread handles rendering (ie not main app thread) but
    // is waiting on this routine before rendering begins
    if (!HasProcessed())
      break;
  }
}

void CGUIDialogProgress::Progress()
{
  if (m_active)
  {
    ProcessRenderLoop();
  }
}

bool CGUIDialogProgress::OnMessage(CGUIMessage& message)
{
  switch ( message.GetMessage() )
  {

  case GUI_MSG_WINDOW_DEINIT:
    Reset();
    break;

  case GUI_MSG_CLICKED:
    {
      int iControl = message.GetSenderId();
      if (iControl >= CONTROL_CHOICES_START && iControl < (CONTROL_CHOICES_START + DIALOG_MAX_CHOICES))
      {
        // special handling for choice 0 mapped to cancel button
        if (m_bCanCancel && !m_supportedChoices[0] && (iControl == CONTROL_CHOICES_START))
        {
          if (m_iChoice != CHOICE_CANCELED)
          {
            std::string strHeading = m_strHeading;
            strHeading.append(" : ");
            strHeading.append(g_localizeStrings.Get(16024));
            CGUIDialogBoxBase::SetHeading(CVariant{strHeading});
            m_iChoice = CHOICE_CANCELED;
          }
        }
        else
        {
          m_iChoice = iControl - CONTROL_CHOICES_START;
        }
        return true;
      }
    }
    break;
  }
  return CGUIDialog::OnMessage(message);
}

bool CGUIDialogProgress::OnBack(int actionID)
{
  if (m_bCanCancel)
    m_iChoice = CHOICE_CANCELED;
  else
    m_iChoice = CHOICE_NONE;

  return true;
}

void CGUIDialogProgress::OnWindowLoaded()
{
  CGUIDialog::OnWindowLoaded();
  CGUIControl *control = GetControl(CONTROL_PROGRESS_BAR);
  if (control && control->GetControlType() == CGUIControl::GUICONTROL_PROGRESS)
  {
    // make sure we have the appropriate info set
    CGUIProgressControl *progress = static_cast<CGUIProgressControl*>(control);
    if (!progress->GetInfo())
      progress->SetInfo(SYSTEM_PROGRESS_BAR);
  }
}

void CGUIDialogProgress::SetPercentage(int iPercentage)
{
  if (iPercentage < 0) iPercentage = 0;
  if (iPercentage > 100) iPercentage = 100;

  if (iPercentage != m_percentage)
    MarkDirtyRegion();

  m_percentage = iPercentage;
}

void CGUIDialogProgress::SetProgressMax(int iMax)
{
  m_iMax=iMax;
  m_iCurrent=0;
}

void CGUIDialogProgress::SetProgressAdvance(int nSteps/*=1*/)
{
  m_iCurrent+=nSteps;

  if (m_iCurrent>m_iMax)
    m_iCurrent=0;

  if (m_iMax > 0)
    SetPercentage((m_iCurrent*100)/m_iMax);
}

bool CGUIDialogProgress::Abort()
{
  return m_active ? IsCanceled() : false;
}

void CGUIDialogProgress::ShowProgressBar(bool bOnOff)
{
  CSingleLock lock(m_section);
  m_showProgress = bOnOff;
  SetInvalid();
}

bool CGUIDialogProgress::Wait(int progresstime /*= 10*/)
{
  CEvent m_done;
  while (!m_done.WaitMSec(progresstime) && m_active && !IsCanceled())
    Progress();

  return !IsCanceled();
}

bool CGUIDialogProgress::WaitOnEvent(CEvent& event)
{
  while (!event.WaitMSec(1))
  {
    if (IsCanceled())
      return false;

    Progress();
  }

  return !IsCanceled();
}

void CGUIDialogProgress::UpdateControls()
{
  // take a copy to save holding the lock for too long
  bool bShowProgress;
  bool bShowCancel;
  std::array<bool, DIALOG_MAX_CHOICES> choices;
  {
    CSingleLock lock(m_section);
    bShowProgress = m_showProgress;
    bShowCancel = m_bCanCancel;
    choices = m_supportedChoices;
  }

  if (bShowProgress)
    SET_CONTROL_VISIBLE(CONTROL_PROGRESS_BAR);
  else
    SET_CONTROL_HIDDEN(CONTROL_PROGRESS_BAR);

  bool bAllHidden = true;
  for (int i = 0; i < DIALOG_MAX_CHOICES; ++i)
  {
    if (choices[i])
    {
      bAllHidden = false;
      SET_CONTROL_VISIBLE(CONTROL_CHOICES_START + i);
    }
    else
      SET_CONTROL_HIDDEN(CONTROL_CHOICES_START + i);
  }

  // special handling for choice 0 mapped to cancel button
  if (bShowCancel && bAllHidden)
    SET_CONTROL_VISIBLE(CONTROL_CHOICES_START);
}

void CGUIDialogProgress::Process(unsigned int currentTime, CDirtyRegionList &dirtyregions)
{
  if (m_bInvalidated)
    UpdateControls();

  CGUIDialogBoxBase::Process(currentTime, dirtyregions);
}

void CGUIDialogProgress::OnInitWindow()
{
  UpdateControls();

  bool bNoFocus = true;
  for (int i = 0; i < DIALOG_MAX_CHOICES; ++i)
  {
    if (m_supportedChoices[i])
    {
      bNoFocus = false;
      SET_CONTROL_FOCUS(CONTROL_CHOICES_START + i, 0);
      break;
    }
  }

  // special handling for choice 0 mapped to cancel button
  if (m_bCanCancel && bNoFocus)
    SET_CONTROL_FOCUS(CONTROL_CHOICES_START,0 );

  CGUIDialogBoxBase::OnInitWindow();
}

int CGUIDialogProgress::GetDefaultLabelID(int controlId) const
{
  // special handling for choice 0 mapped to cancel button
  if (m_bCanCancel && !m_supportedChoices[0] && (controlId == CONTROL_CHOICES_START))
    return 222; // Cancel

  return CGUIDialogBoxBase::GetDefaultLabelID(controlId);
}
