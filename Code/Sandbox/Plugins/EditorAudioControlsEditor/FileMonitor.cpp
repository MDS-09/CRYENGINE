// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FileMonitor.h"

#include "IEditorImpl.h"
#include "SystemAssetsManager.h"
#include "AudioControlsEditorPlugin.h"

#include <CryString/CryPath.h>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CFileMonitor::CFileMonitor(int const delay, QObject* const pParent)
	: QTimer(pParent)
	, m_delay(delay)
{
	setSingleShot(true);
	connect(this, &CFileMonitor::timeout, this, &CFileMonitor::SignalReloadData);
}

//////////////////////////////////////////////////////////////////////////
CFileMonitor::~CFileMonitor()
{
	stop();
	GetIEditor()->GetFileMonitor()->UnregisterListener(this);
}

//////////////////////////////////////////////////////////////////////////
void CFileMonitor::OnFileChange(char const* filename, EChangeType eType)
{
	start(m_delay);
}

//////////////////////////////////////////////////////////////////////////
void CFileMonitor::Disable()
{
	stop();
	GetIEditor()->GetFileMonitor()->UnregisterListener(this);
}

//////////////////////////////////////////////////////////////////////////
CFileMonitorSystem::CFileMonitorSystem(int const delay, QObject* const pParent)
	: CFileMonitor(delay, pParent)
	, m_assetFolder (Utils::GetAssetFolder())
	, m_delayTimer(new QTimer(this))
{
	m_delayTimer->setSingleShot(true);
	connect(m_delayTimer, &QTimer::timeout, this, &CFileMonitorSystem::Enable);
	Enable();
}

//////////////////////////////////////////////////////////////////////////
void CFileMonitorSystem::Enable()
{
	GetIEditor()->GetFileMonitor()->RegisterListener(this, m_assetFolder, "xml");
}

//////////////////////////////////////////////////////////////////////////
void CFileMonitorSystem::EnableDelayed()
{
	m_delayTimer->start(m_delay);
}

//////////////////////////////////////////////////////////////////////////
CFileMonitorMiddleware::CFileMonitorMiddleware(int const delay, QObject* const pParent)
	: CFileMonitor(delay, pParent)
{
	Enable();
}

//////////////////////////////////////////////////////////////////////////
void CFileMonitorMiddleware::Enable()
{
	IEditorImpl* const pEditorImpl = CAudioControlsEditorPlugin::GetImplEditor();

	if (pEditorImpl != nullptr)
	{
		IImplSettings const* const pImplSettings = pEditorImpl->GetSettings();

		if (pImplSettings != nullptr)
		{
			stop();
			m_monitorFolders.clear();

			int const gameFolderPathLength = (PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR).GetLength();

			string const& projectPath = pImplSettings->GetProjectPath();
			string const& projectPathSubstr = projectPath.substr(gameFolderPathLength);
			m_monitorFolders.emplace_back(projectPathSubstr.c_str());

			string const& soundBanksPath = pImplSettings->GetSoundBanksPath();
			string const& soundBanksPathSubstr = (soundBanksPath).substr(gameFolderPathLength);
			m_monitorFolders.emplace_back(soundBanksPathSubstr.c_str());

			string const& localizationPath = PathUtil::GetLocalizationFolder();
			m_monitorFolders.emplace_back(localizationPath.c_str());

			GetIEditor()->GetFileMonitor()->UnregisterListener(this);

			for (auto const& folder : m_monitorFolders)
			{
				GetIEditor()->GetFileMonitor()->RegisterListener(this, folder);
			}
		}
	}
}
} // namespace ACE
