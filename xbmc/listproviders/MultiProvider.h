/*
 *  Copyright (C) 2013-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "IListProvider.h"
#include "threads/CriticalSection.h"

#include <map>
#include <vector>

typedef std::unique_ptr<IListProvider> IListProviderPtr;

/*!
 \ingroup listproviders
 \brief A listprovider that handles multiple individual providers.
 */
class CMultiProvider : public IListProvider
{
public:
  CMultiProvider(const TiXmlNode *first, int parentID);
  explicit CMultiProvider(const CMultiProvider& other);

  // Implementation of IListProvider
  std::unique_ptr<IListProvider> Clone() override;
  bool Update(bool forceRefresh) override;
  void Fetch(std::vector<CGUIListItemPtr> &items) override;
  bool IsUpdating() const override;
  void Reset() override;
  bool OnClick(const CGUIListItemPtr &item) override;
  bool OnInfo(const CGUIListItemPtr &item) override;
  bool OnContextMenu(const CGUIListItemPtr &item) override;

protected:
  typedef size_t item_key_type;
  static item_key_type GetItemKey(CGUIListItemPtr const &item);
  std::vector<IListProviderPtr> m_providers;
  std::map<item_key_type, IListProvider*> m_itemMap;
  CCriticalSection m_section; // protects m_itemMap
};
