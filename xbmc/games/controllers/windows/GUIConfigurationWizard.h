/*
 *  Copyright (C) 2014-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "IConfigurationWindow.h"
#include "games/controllers/ControllerFeature.h"
#include "input/XBMC_keysym.h"
#include "input/joysticks/DriverPrimitive.h"
#include "input/joysticks/interfaces/IButtonMapper.h"
#include "input/keyboard/interfaces/IKeyboardDriverHandler.h"
#include "threads/CriticalSection.h"
#include "threads/Event.h"
#include "threads/Thread.h"
#include "utils/Observer.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace KODI
{
namespace KEYBOARD
{
class IActionMap;
}

namespace GAME
{
class CGUIConfigurationWizard : public IConfigurationWizard,
                                public JOYSTICK::IButtonMapper,
                                public KEYBOARD::IKeyboardDriverHandler,
                                public Observer,
                                protected CThread
{
public:
  CGUIConfigurationWizard();

  ~CGUIConfigurationWizard() override;

  // implementation of IConfigurationWizard
  void Run(const std::string& strControllerId,
           const std::vector<IFeatureButton*>& buttons) override;
  void OnUnfocus(IFeatureButton* button) override;
  bool Abort(bool bWait = true) override;
  void RegisterKey(const CControllerFeature& key) override;
  void UnregisterKeys() override;

  // implementation of IButtonMapper
  std::string ControllerID() const override { return m_strControllerId; }
  bool NeedsCooldown() const override { return true; }
  bool AcceptsPrimitive(JOYSTICK::PRIMITIVE_TYPE type) const override { return true; }
  bool MapPrimitive(JOYSTICK::IButtonMap* buttonMap,
                    IKeymap* keymap,
                    const JOYSTICK::CDriverPrimitive& primitive) override;
  void OnEventFrame(const JOYSTICK::IButtonMap* buttonMap, bool bMotion) override;
  void OnLateAxis(const JOYSTICK::IButtonMap* buttonMap, unsigned int axisIndex) override;

  // implementation of IKeyboardDriverHandler
  bool OnKeyPress(const CKey& key) override;
  void OnKeyRelease(const CKey& key) override {}

  // implementation of Observer
  void Notify(const Observable& obs, const ObservableMessage msg) override;

protected:
  // implementation of CThread
  void Process() override;

private:
  void InitializeState(void);

  bool IsMapping() const;
  bool IsMapping(const std::string& location) const;

  void InstallHooks(void);
  void RemoveHooks(void);

  void OnMotion(const JOYSTICK::IButtonMap* buttonMap);
  void OnMotionless(const JOYSTICK::IButtonMap* buttonMap);

  bool OnAction(unsigned int actionId);

  // Run() parameters
  std::string m_strControllerId;
  std::vector<IFeatureButton*> m_buttons;

  // State variables and mutex
  IFeatureButton* m_currentButton;
  INPUT::CARDINAL_DIRECTION m_cardinalDirection;
  JOYSTICK::WHEEL_DIRECTION m_wheelDirection;
  JOYSTICK::THROTTLE_DIRECTION m_throttleDirection;
  std::set<JOYSTICK::CDriverPrimitive> m_history; // History to avoid repeated features
  bool m_lateAxisDetected; // Set to true if an axis is detected during button mapping
  std::string m_location; // Peripheral location of device that we're mapping
  bool m_bIsKeyboard = false; // True if we're mapping keyboard keys
  CCriticalSection m_stateMutex;

  // Synchronization events
  CEvent m_inputEvent;
  CEvent m_motionlessEvent;
  CCriticalSection m_motionMutex;
  std::set<const JOYSTICK::IButtonMap*> m_bInMotion;

  // Keyboard handling
  std::unique_ptr<KEYBOARD::IActionMap> m_actionMap;
  std::map<XBMCKey, CControllerFeature> m_keyMap; // Keycode -> feature
};
} // namespace GAME
} // namespace KODI
