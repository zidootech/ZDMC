/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIEditControl.h"

#include "GUIFont.h"
#include "GUIKeyboardFactory.h"
#include "GUIUserMessages.h"
#include "GUIWindowManager.h"
#include "LocalizeStrings.h"
#include "ServiceBroker.h"
#include "XBDateTime.h"
#include "dialogs/GUIDialogNumeric.h"
#include "input/Key.h"
#include "input/XBMC_vkeys.h"
#include "utils/CharsetConverter.h"
#include "utils/Color.h"
#include "utils/Digest.h"
#include "utils/Variant.h"
#include "windowing/WinSystem.h"

using namespace KODI::GUILIB;

using KODI::UTILITY::CDigest;

const char* CGUIEditControl::smsLetters[10] = { " !@#$%^&*()[]{}<>/\\|0", ".,;:\'\"-+_=?`~1", "abc2ABC", "def3DEF", "ghi4GHI", "jkl5JKL", "mno6MNO", "pqrs7PQRS", "tuv8TUV", "wxyz9WXYZ" };
const unsigned int CGUIEditControl::smsDelay = 1000;

#ifdef TARGET_WINDOWS
extern HWND g_hWnd;
#endif

CGUIEditControl::CGUIEditControl(int parentID, int controlID, float posX, float posY,
                                 float width, float height, const CTextureInfo &textureFocus, const CTextureInfo &textureNoFocus,
                                 const CLabelInfo& labelInfo, const std::string &text)
    : CGUIButtonControl(parentID, controlID, posX, posY, width, height, textureFocus, textureNoFocus, labelInfo)
{
  DefaultConstructor();
  SetLabel(text);
}

void CGUIEditControl::DefaultConstructor()
{
  ControlType = GUICONTROL_EDIT;
  m_textOffset = 0;
  m_textWidth = GetWidth();
  m_cursorPos = 0;
  m_cursorBlink = 0;
  m_inputHeading = g_localizeStrings.Get(16028);
  m_inputType = INPUT_TYPE_TEXT;
  m_smsLastKey = 0;
  m_smsKeyIndex = 0;
  m_label.SetAlign(m_label.GetLabelInfo().align & XBFONT_CENTER_Y); // left align
  m_label2.GetLabelInfo().offsetX = 0;
  m_isMD5 = false;
  m_invalidInput = false;
  m_inputValidator = NULL;
  m_inputValidatorData = NULL;
  m_editLength = 0;
  m_editOffset = 0;
}

CGUIEditControl::CGUIEditControl(const CGUIButtonControl &button)
    : CGUIButtonControl(button)
{
  DefaultConstructor();
}

CGUIEditControl::~CGUIEditControl(void) = default;

bool CGUIEditControl::OnMessage(CGUIMessage &message)
{
  if (message.GetMessage() == GUI_MSG_SET_TYPE)
  {
    SetInputType((INPUT_TYPE)message.GetParam1(), message.GetParam2());
    return true;
  }
  else if (message.GetMessage() == GUI_MSG_ITEM_SELECTED)
  {
    message.SetLabel(GetLabel2());
    return true;
  }
  else if (message.GetMessage() == GUI_MSG_SET_TEXT &&
          ((message.GetControlId() <= 0 && HasFocus()) || (message.GetControlId() == GetID())))
  {
    SetLabel2(message.GetLabel());
    UpdateText();
  }
  return CGUIButtonControl::OnMessage(message);
}

bool CGUIEditControl::OnAction(const CAction &action)
{
  ValidateCursor();

  if (m_inputType != INPUT_TYPE_READONLY)
  {
    if (action.GetID() == ACTION_BACKSPACE)
    {
      // backspace
      if (m_cursorPos)
      {
        if (!ClearMD5())
          m_text2.erase(--m_cursorPos, 1);
        UpdateText();
      }
      return true;
    }
    else if (action.GetID() == ACTION_MOVE_LEFT ||
             action.GetID() == ACTION_CURSOR_LEFT)
    {
      if (m_cursorPos > 0)
      {
        m_cursorPos--;
        UpdateText(false);
        return true;
      }
    }
    else if (action.GetID() == ACTION_MOVE_RIGHT ||
             action.GetID() == ACTION_CURSOR_RIGHT)
    {
      if (m_cursorPos < m_text2.size())
      {
        m_cursorPos++;
        UpdateText(false);
        return true;
      }
    }
    else if (action.GetID() == ACTION_PASTE)
    {
      ClearMD5();
      OnPasteClipboard();
      return true;
    }
    else if (action.GetID() >= KEY_VKEY && action.GetID() < KEY_UNICODE && m_edit.empty())
    {
      // input from the keyboard (vkey, not ascii)
      unsigned char b = action.GetID() & 0xFF;
      if (b == XBMCVK_HOME)
      {
        m_cursorPos = 0;
        UpdateText(false);
        return true;
      }
      else if (b == XBMCVK_END)
      {
        m_cursorPos = m_text2.length();
        UpdateText(false);
        return true;
      }
      if (b == XBMCVK_LEFT && m_cursorPos > 0)
      {
        m_cursorPos--;
        UpdateText(false);
        return true;
      }
      if (b == XBMCVK_RIGHT && m_cursorPos < m_text2.length())
      {
        m_cursorPos++;
        UpdateText(false);
        return true;
      }
      if (b == XBMCVK_DELETE)
      {
        if (m_cursorPos < m_text2.length())
        {
          if (!ClearMD5())
            m_text2.erase(m_cursorPos, 1);
          UpdateText();
          return true;
        }
      }
      if (b == XBMCVK_BACK)
      {
        if (m_cursorPos > 0)
        {
          if (!ClearMD5())
            m_text2.erase(--m_cursorPos, 1);
          UpdateText();
        }
        return true;
      }
      else if (b == XBMCVK_RETURN || b == XBMCVK_NUMPADENTER)
      {
        // enter - send click message, but otherwise ignore
        SEND_CLICK_MESSAGE(GetID(), GetParentID(), 1);
        return true;
      }
      else if (b == XBMCVK_ESCAPE)
      { // escape - fallthrough to default action
        return CGUIButtonControl::OnAction(action);
      }
    }
    else if (action.GetID() == KEY_UNICODE)
    {
      // input from the keyboard
      int ch = action.GetUnicode();
      // ignore non-printing characters
      if ( !((0 <= ch && ch < 0x8) || (0xE <= ch && ch < 0x1B) || (0x1C <= ch && ch < 0x20)) )
      {
      switch (ch)
      {
      case 9:  // tab, ignore
      case 11: // Non-printing character, ignore
      case 12: // Non-printing character, ignore
        break;
      case 10:
      case 13:
        {
          // enter - send click message, but otherwise ignore
          SEND_CLICK_MESSAGE(GetID(), GetParentID(), 1);
          return true;
        }
      case 27:
        { // escape - fallthrough to default action
          return CGUIButtonControl::OnAction(action);
        }
      case 8:
        {
          // backspace
          if (m_cursorPos)
          {
            if (!ClearMD5())
              m_text2.erase(--m_cursorPos, 1);
          }
          break;
        }
      case 127:
        { // delete
          if (m_cursorPos < m_text2.length())
          {
            if (!ClearMD5())
              m_text2.erase(m_cursorPos, 1);
          }
        break;
        }
      default:
        {
          ClearMD5();
          m_edit.clear();
          m_text2.insert(m_text2.begin() + m_cursorPos++, action.GetUnicode());
          break;
        }
      }
      UpdateText();
      return true;
      }
    }
    else if (action.GetID() >= REMOTE_0 && action.GetID() <= REMOTE_9)
    { // input from the remote
      ClearMD5();
      m_edit.clear();
      OnSMSCharacter(action.GetID() - REMOTE_0);
      return true;
    }
    else if (action.GetID() == ACTION_INPUT_TEXT)
    {
      m_edit.clear();
      std::wstring str;
      g_charsetConverter.utf8ToW(action.GetText(), str, false);
      m_text2.insert(m_cursorPos, str);
      m_cursorPos += str.size();
      UpdateText();
      return true;
    }
  }
  return CGUIButtonControl::OnAction(action);
}

void CGUIEditControl::OnClick()
{
  // we received a click - it's not from the keyboard, so pop up the virtual keyboard, unless
  // that is where we reside!
  if (GetParentID() == WINDOW_DIALOG_KEYBOARD)
    return;

  std::string utf8;
  g_charsetConverter.wToUTF8(m_text2, utf8);
  bool textChanged = false;
  switch (m_inputType)
  {
    case INPUT_TYPE_READONLY:
      textChanged = false;
      break;
    case INPUT_TYPE_NUMBER:
      textChanged = CGUIDialogNumeric::ShowAndGetNumber(utf8, m_inputHeading);
      break;
    case INPUT_TYPE_SECONDS:
      textChanged = CGUIDialogNumeric::ShowAndGetSeconds(utf8, g_localizeStrings.Get(21420));
      break;
    case INPUT_TYPE_TIME:
    {
      CDateTime dateTime;
      dateTime.SetFromDBTime(utf8);
      KODI::TIME::SystemTime time;
      dateTime.GetAsSystemTime(time);
      if (CGUIDialogNumeric::ShowAndGetTime(time, !m_inputHeading.empty() ? m_inputHeading : g_localizeStrings.Get(21420)))
      {
        dateTime = CDateTime(time);
        utf8 = dateTime.GetAsLocalizedTime("", false);
        textChanged = true;
      }
      break;
    }
    case INPUT_TYPE_DATE:
    {
      CDateTime dateTime;
      dateTime.SetFromDBDate(utf8);
      if (dateTime < CDateTime(2000,1, 1, 0, 0, 0))
        dateTime = CDateTime(2000, 1, 1, 0, 0, 0);
      KODI::TIME::SystemTime date;
      dateTime.GetAsSystemTime(date);
      if (CGUIDialogNumeric::ShowAndGetDate(date, !m_inputHeading.empty() ? m_inputHeading : g_localizeStrings.Get(21420)))
      {
        dateTime = CDateTime(date);
        utf8 = dateTime.GetAsDBDate();
        textChanged = true;
      }
      break;
    }
    case INPUT_TYPE_IPADDRESS:
      textChanged = CGUIDialogNumeric::ShowAndGetIPAddress(utf8, m_inputHeading);
      break;
    case INPUT_TYPE_SEARCH:
      textChanged = CGUIKeyboardFactory::ShowAndGetFilter(utf8, true);
      break;
    case INPUT_TYPE_FILTER:
      textChanged = CGUIKeyboardFactory::ShowAndGetFilter(utf8, false);
      break;
    case INPUT_TYPE_PASSWORD_NUMBER_VERIFY_NEW:
      textChanged = CGUIDialogNumeric::ShowAndVerifyNewPassword(utf8);
      break;
    case INPUT_TYPE_PASSWORD_MD5:
      utf8 = ""; //! @todo Ideally we'd send this to the keyboard and tell the keyboard we have this type of input
      // fallthrough
    case INPUT_TYPE_TEXT:
    default:
      textChanged = CGUIKeyboardFactory::ShowAndGetInput(utf8, m_inputHeading, true, m_inputType == INPUT_TYPE_PASSWORD || m_inputType == INPUT_TYPE_PASSWORD_MD5);
      break;
  }
  if (textChanged)
  {
    ClearMD5();
    m_edit.clear();
    g_charsetConverter.utf8ToW(utf8, m_text2);
    m_cursorPos = m_text2.size();
    UpdateText();
    m_cursorPos = m_text2.size();
  }
}

void CGUIEditControl::UpdateText(bool sendUpdate)
{
  m_smsTimer.Stop();
  if (sendUpdate)
  {
    ValidateInput();

    SEND_CLICK_MESSAGE(GetID(), GetParentID(), 0);

    m_textChangeActions.ExecuteActions(GetID(), GetParentID());
  }
  SetInvalid();
}

void CGUIEditControl::SetInputType(CGUIEditControl::INPUT_TYPE type, const CVariant& heading)
{
  m_inputType = type;
  if (heading.isString())
    m_inputHeading = heading.asString();
  else if (heading.isInteger() && heading.asInteger())
    m_inputHeading = g_localizeStrings.Get(static_cast<uint32_t>(heading.asInteger()));
  //! @todo Verify the current input string?
}

void CGUIEditControl::RecalcLabelPosition()
{
  // ensure that our cursor is within our width
  ValidateCursor();

  std::wstring text = GetDisplayedText();
  m_textWidth = m_label.CalcTextWidth(text + L'|');
  float beforeCursorWidth = m_label.CalcTextWidth(text.substr(0, m_cursorPos));
  float afterCursorWidth = m_label.CalcTextWidth(text.substr(0, m_cursorPos) + L'|');
  float leftTextWidth = m_label.GetRenderRect().Width();
  float maxTextWidth = m_label.GetMaxWidth();
  if (leftTextWidth > 0)
    maxTextWidth -= leftTextWidth + spaceWidth;

  // if skinner forgot to set height :p
  if (m_height == 0 && m_label.GetLabelInfo().font)
    m_height = m_label.GetLabelInfo().font->GetTextHeight(1);

  if (m_textWidth > maxTextWidth)
  { // we render taking up the full width, so make sure our cursor position is
    // within the render window
    if (m_textOffset + afterCursorWidth > maxTextWidth)
    {
      // move the position to the left (outside of the viewport)
      m_textOffset = maxTextWidth - afterCursorWidth;
    }
    else if (m_textOffset + beforeCursorWidth < 0) // offscreen to the left
    {
      // otherwise use original position
      m_textOffset = -beforeCursorWidth;
    }
    else if (m_textOffset + m_textWidth < maxTextWidth)
    { // we have more text than we're allowed, but we aren't filling all the space
      m_textOffset = maxTextWidth - m_textWidth;
    }
  }
  else
    m_textOffset = 0;
}

void CGUIEditControl::ProcessText(unsigned int currentTime)
{
  if (m_smsTimer.IsRunning() && m_smsTimer.GetElapsedMilliseconds() > smsDelay)
    UpdateText();

  if (m_bInvalidated)
  {
    m_label.SetMaxRect(m_posX, m_posY, m_width, m_height);
    m_label.SetText(m_info.GetLabel(GetParentID()));
    RecalcLabelPosition();
  }

  bool changed = false;

  m_clipRect.x1 = m_label.GetRenderRect().x1;
  m_clipRect.x2 = m_clipRect.x1 + m_label.GetMaxWidth();
  m_clipRect.y1 = m_posY;
  m_clipRect.y2 = m_posY + m_height;

  // start by rendering the normal text
  float leftTextWidth = m_label.GetRenderRect().Width();
  if (leftTextWidth > 0)
  {
    // render the text on the left
    changed |= m_label.SetColor(GetTextColor());
    changed |= m_label.Process(currentTime);

    m_clipRect.x1 += leftTextWidth + spaceWidth;
  }

  if (CServiceBroker::GetWinSystem()->GetGfxContext().SetClipRegion(m_clipRect.x1, m_clipRect.y1, m_clipRect.Width(), m_clipRect.Height()))
  {
    uint32_t align = m_label.GetLabelInfo().align & XBFONT_CENTER_Y; // start aligned left
    if (m_label2.GetTextWidth() < m_clipRect.Width())
    { // align text as our text fits
      if (leftTextWidth > 0)
      { // right align as we have 2 labels
        align |= XBFONT_RIGHT;
      }
      else
      { // align by whatever the skinner requests
        align |= (m_label2.GetLabelInfo().align & 3);
      }
    }
    changed |= m_label2.SetMaxRect(m_clipRect.x1 + m_textOffset, m_posY, m_clipRect.Width() - m_textOffset, m_height);

    std::wstring text = GetDisplayedText();
    std::string hint = m_hintInfo.GetLabel(GetParentID());

    if (!HasFocus() && text.empty() && !hint.empty())
    {
      changed |= m_label2.SetText(hint);
    }
    else if ((HasFocus() || GetParentID() == WINDOW_DIALOG_KEYBOARD) &&
             m_inputType != INPUT_TYPE_READONLY)
    {
      changed |= SetStyledText(text);
    }
    else
      changed |= m_label2.SetTextW(text);

    changed |= m_label2.SetAlign(align);
    changed |= m_label2.SetColor(GetTextColor());
    changed |= m_label2.SetOverflow(CGUILabel::OVER_FLOW_CLIP);
    changed |= m_label2.Process(currentTime);
    CServiceBroker::GetWinSystem()->GetGfxContext().RestoreClipRegion();
  }
  if (changed)
    MarkDirtyRegion();
}

void CGUIEditControl::RenderText()
{
  m_label.Render();

  if (CServiceBroker::GetWinSystem()->GetGfxContext().SetClipRegion(m_clipRect.x1, m_clipRect.y1, m_clipRect.Width(), m_clipRect.Height()))
  {
    m_label2.Render();
    CServiceBroker::GetWinSystem()->GetGfxContext().RestoreClipRegion();
  }
}

CGUILabel::COLOR CGUIEditControl::GetTextColor() const
{
  CGUILabel::COLOR color = CGUIButtonControl::GetTextColor();
  if (color != CGUILabel::COLOR_DISABLED && HasInvalidInput())
    return CGUILabel::COLOR_INVALID;

  return color;
}

void CGUIEditControl::SetHint(const GUIINFO::CGUIInfoLabel& hint)
{
  m_hintInfo = hint;
}

std::wstring CGUIEditControl::GetDisplayedText() const
{
  std::wstring text(m_text2);
  if (m_inputType == INPUT_TYPE_PASSWORD || m_inputType == INPUT_TYPE_PASSWORD_MD5 || m_inputType == INPUT_TYPE_PASSWORD_NUMBER_VERIFY_NEW)
  {
    text.clear();
    if (m_smsTimer.IsRunning())
    { // using the remove to input, so display the last key input
      text.append(m_cursorPos - 1, L'*');
      text.append(1, m_text2[m_cursorPos - 1]);
      text.append(m_text2.size() - m_cursorPos, L'*');
    }
    else
      text.append(m_text2.size(), L'*');
  }
  else if (!m_edit.empty())
    text.insert(m_editOffset, m_edit);
  return text;
}

bool CGUIEditControl::SetStyledText(const std::wstring &text)
{
  vecText styled;
  styled.reserve(text.size() + 1);

  std::vector<UTILS::Color> colors;
  colors.push_back(m_label.GetLabelInfo().textColor);
  colors.push_back(m_label.GetLabelInfo().disabledColor);
  UTILS::Color select = m_label.GetLabelInfo().selectedColor;
  if (!select)
    select = 0xFFFF0000;
  colors.push_back(select);
  colors.push_back(0x00FFFFFF);

  unsigned int startHighlight = m_cursorPos;
  unsigned int endHighlight   = m_cursorPos + m_edit.size();
  unsigned int startSelection = m_cursorPos + m_editOffset;
  unsigned int endSelection   = m_cursorPos + m_editOffset + m_editLength;

  CGUIFont* font = m_label2.GetLabelInfo().font;
  uint32_t style = (font ? font->GetStyle() : (FONT_STYLE_NORMAL & FONT_STYLE_MASK)) << 24;

  for (unsigned int i = 0; i < text.size(); i++)
  {
    uint32_t ch = text[i] | style;
    if (m_editLength > 0 && startSelection <= i && i < endSelection)
      ch |= (2 << 16); // highlight the letters we're playing with
    else if (!m_edit.empty() && (i < startHighlight || i >= endHighlight))
      ch |= (1 << 16); // dim the bits we're not editing
    styled.push_back(ch);
  }

  // show the cursor
  uint32_t ch = L'|' | style;
  if ((++m_cursorBlink % 64) > 32)
    ch |= (3 << 16);
  styled.insert(styled.begin() + m_cursorPos, ch);

  return m_label2.SetStyledText(styled, colors);
}

void CGUIEditControl::ValidateCursor()
{
  if (m_cursorPos > m_text2.size())
    m_cursorPos = m_text2.size();
}

void CGUIEditControl::SetLabel(const std::string &text)
{
  CGUIButtonControl::SetLabel(text);
  SetInvalid();
}

void CGUIEditControl::SetLabel2(const std::string &text)
{
  m_edit.clear();
  std::wstring newText;
  g_charsetConverter.utf8ToW(text, newText, false);
  if (newText != m_text2)
  {
    m_isMD5 = (m_inputType == INPUT_TYPE_PASSWORD_MD5 || m_inputType == INPUT_TYPE_PASSWORD_NUMBER_VERIFY_NEW);
    m_text2 = newText;
    m_cursorPos = m_text2.size();
    ValidateInput();
    SetInvalid();
  }
}

std::string CGUIEditControl::GetLabel2() const
{
  std::string text;
  g_charsetConverter.wToUTF8(m_text2, text);
  if (m_inputType == INPUT_TYPE_PASSWORD_MD5 && !m_isMD5)
    return CDigest::Calculate(CDigest::Type::MD5, text);
  return text;
}

bool CGUIEditControl::ClearMD5()
{
  if (!(m_inputType == INPUT_TYPE_PASSWORD_MD5 || m_inputType == INPUT_TYPE_PASSWORD_NUMBER_VERIFY_NEW) || !m_isMD5)
    return false;

  m_text2.clear();
  m_cursorPos = 0;
  if (m_inputType != INPUT_TYPE_PASSWORD_NUMBER_VERIFY_NEW)
    m_isMD5 = false;
  return true;
}

unsigned int CGUIEditControl::GetCursorPosition() const
{
  return m_cursorPos;
}

void CGUIEditControl::SetCursorPosition(unsigned int iPosition)
{
  m_cursorPos = iPosition;
}

void CGUIEditControl::OnSMSCharacter(unsigned int key)
{
  assert(key < 10);
  if (m_smsTimer.IsRunning())
  {
    // we're already entering an SMS character
    if (key != m_smsLastKey || m_smsTimer.GetElapsedMilliseconds() > smsDelay)
    { // a different key was clicked than last time, or we have timed out
      m_smsLastKey = key;
      m_smsKeyIndex = 0;
    }
    else
    { // same key as last time within the appropriate time period
      m_smsKeyIndex++;
      if (m_cursorPos)
        m_text2.erase(--m_cursorPos, 1);
    }
  }
  else
  { // key is pressed for the first time
    m_smsLastKey = key;
    m_smsKeyIndex = 0;
  }

  m_smsKeyIndex = m_smsKeyIndex % strlen(smsLetters[key]);

  m_text2.insert(m_text2.begin() + m_cursorPos++, smsLetters[key][m_smsKeyIndex]);
  UpdateText();
  m_smsTimer.StartZero();
}

void CGUIEditControl::OnPasteClipboard()
{
  std::wstring unicode_text;
  std::string utf8_text;

  // Get text from the clipboard
  utf8_text = CServiceBroker::GetWinSystem()->GetClipboardText();
  g_charsetConverter.utf8ToW(utf8_text, unicode_text);

  // Insert the pasted text at the current cursor position.
  if (unicode_text.length() > 0)
  {
    std::wstring left_end = m_text2.substr(0, m_cursorPos);
    std::wstring right_end = m_text2.substr(m_cursorPos);

    m_text2 = left_end;
    m_text2.append(unicode_text);
    m_text2.append(right_end);
    m_cursorPos += unicode_text.length();
    UpdateText();
  }
}

void CGUIEditControl::SetInputValidation(StringValidation::Validator inputValidator, void *data /* = NULL */)
{
  if (m_inputValidator == inputValidator)
    return;

  m_inputValidator = inputValidator;
  m_inputValidatorData = data;
  // the input validator has changed, so re-validate the current data
  ValidateInput();
}

bool CGUIEditControl::ValidateInput(const std::wstring &data) const
{
  if (m_inputValidator == NULL)
    return true;

  return m_inputValidator(GetLabel2(), m_inputValidatorData != NULL ? m_inputValidatorData : const_cast<void*>((const void*)this));
}

void CGUIEditControl::ValidateInput()
{
  // validate the input
  bool invalid = !ValidateInput(m_text2);
  // nothing to do if still valid/invalid
  if (invalid != m_invalidInput)
  {
    // the validity state has changed so we need to update the control
    m_invalidInput = invalid;

    // let the window/dialog know that the validity has changed
    CGUIMessage msg(GUI_MSG_VALIDITY_CHANGED, GetID(), GetID(), m_invalidInput ? 0 : 1);
    SendWindowMessage(msg);

    SetInvalid();
  }
}

void CGUIEditControl::SetFocus(bool focus)
{
  m_smsTimer.Stop();
  CGUIControl::SetFocus(focus);
  SetInvalid();
}

std::string CGUIEditControl::GetDescriptionByIndex(int index) const
{
  if (index == 0)
    return GetDescription();
  else if(index == 1)
    return GetLabel2();

  return "";
}
