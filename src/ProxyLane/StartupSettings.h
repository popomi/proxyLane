#pragma once

#include "ConfigStorage.h"
#include <winreg.h>

namespace StartupSettings
{
	inline CString GetPreferredExecutablePath()
	{
		WCHAR buffer[32768] = {};
		const DWORD length = GetModuleFileNameW(NULL, buffer, _countof(buffer));
		if (!length || length >= _countof(buffer))
			return CString();

		CString path(buffer);
#ifndef _WIN64
		BOOL wow64 = FALSE;
		if (IsWow64Process(GetCurrentProcess(), &wow64) && wow64)
		{
			const CString suffix = L"ProxyLane.exe";
			if (path.GetLength() >= suffix.GetLength() &&
				path.Right(suffix.GetLength()).CompareNoCase(suffix) == 0)
			{
				CString candidate = path.Left(path.GetLength() - 4) + L"64.exe";
				if (GetFileAttributesW(candidate) != INVALID_FILE_ATTRIBUTES)
					return candidate;
			}
		}
#endif
		return path;
	}

	inline CString BuildStartupCommand(bool silent)
	{
		return L"\"" + GetPreferredExecutablePath() + (silent ? L"\" --startup --silent" : L"\" --startup");
	}

	inline bool IsRegistryAutoStartEnabled()
	{
		HKEY key = NULL;
		if (RegOpenKeyExW(
			HKEY_CURRENT_USER,
			L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
			0,
			KEY_QUERY_VALUE,
			&key) != ERROR_SUCCESS)
		{
			return false;
		}

		WCHAR value[2048] = {};
		DWORD type = 0;
		DWORD size = sizeof(value);
		const LSTATUS status = RegQueryValueExW(
			key,
			L"ProxyLane",
			NULL,
			&type,
			reinterpret_cast<LPBYTE>(value),
			&size);
		RegCloseKey(key);
		return status == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && value[0] != L'\0';
	}

	inline bool SetRegistryAutoStart(bool enabled, bool silent)
	{
		HKEY key = NULL;
		if (RegCreateKeyExW(
			HKEY_CURRENT_USER,
			L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
			0,
			NULL,
			REG_OPTION_NON_VOLATILE,
			KEY_SET_VALUE,
			NULL,
			&key,
			NULL) != ERROR_SUCCESS)
		{
			return false;
		}

		LSTATUS status = ERROR_SUCCESS;
		if (enabled)
		{
			const CString command = BuildStartupCommand(silent);
			const DWORD bytes = static_cast<DWORD>((command.GetLength() + 1) * sizeof(WCHAR));
			status = RegSetValueExW(
				key,
				L"ProxyLane",
				0,
				REG_SZ,
				reinterpret_cast<const BYTE*>(static_cast<LPCWSTR>(command)),
				bytes);
		}
		else
		{
			status = RegDeleteValueW(key, L"ProxyLane");
			if (status == ERROR_FILE_NOT_FOUND)
				status = ERROR_SUCCESS;
		}

		RegCloseKey(key);
		return status == ERROR_SUCCESS;
	}

	inline bool IsSilentStartEnabled()
	{
		return GetPrivateProfileIntW(L"options", L"SilentStart", 0, ConfigStorage::Path()) != 0;
	}

	inline bool SetSilentStartEnabled(bool enabled)
	{
		const bool saved = ConfigStorage::WriteValue(
			L"options",
			L"SilentStart",
			enabled ? L"1" : L"0",
			ConfigStorage::Path()) != FALSE;
		if (GetPrivateProfileIntW(L"options", L"AutoStart", 0, ConfigStorage::Path()) != 0 ||
			IsRegistryAutoStartEnabled())
		{
			SetRegistryAutoStart(true, enabled);
		}
		return saved;
	}

	inline bool IsAutoStartEnabled()
	{
		const int configured = GetPrivateProfileIntW(L"options", L"AutoStart", -1, ConfigStorage::Path());
		if (configured == -1)
			return IsRegistryAutoStartEnabled();
		return configured != 0;
	}

	inline bool SetAutoStartEnabled(bool enabled)
	{
		const bool regOk = SetRegistryAutoStart(enabled, IsSilentStartEnabled());
		const bool iniOk = ConfigStorage::WriteValue(
			L"options",
			L"AutoStart",
			enabled ? L"1" : L"0",
			ConfigStorage::Path()) != FALSE;
		return regOk && iniOk;
	}

	inline bool IsCloseToTrayEnabled()
	{
		return GetPrivateProfileIntW(L"options", L"CloseToTray", 1, ConfigStorage::Path()) != 0;
	}

	inline bool SetCloseToTrayEnabled(bool enabled)
	{
		return ConfigStorage::WriteValue(
			L"options",
			L"CloseToTray",
			enabled ? L"1" : L"0",
			ConfigStorage::Path()) != FALSE;
	}

	inline CString GetDefaultProfile()
	{
		WCHAR buf[512] = {};
		GetPrivateProfileStringW(
			L"options",
			L"DefaultProfile",
			L"",
			buf,
			_countof(buf),
			ConfigStorage::Path());
		CString profile(buf);
		profile.Trim();
		return profile;
	}

	inline bool SetDefaultProfile(const CString& profileName)
	{
		return ConfigStorage::WriteValue(
			L"options",
			L"DefaultProfile",
			profileName,
			ConfigStorage::Path()) != FALSE;
	}

	inline void SyncRegistryOnStartup()
	{
		if (IsAutoStartEnabled())
			SetRegistryAutoStart(true, IsSilentStartEnabled());
	}
}
