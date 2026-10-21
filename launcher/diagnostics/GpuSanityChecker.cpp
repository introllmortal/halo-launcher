// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Halo Launcher - Minecraft Launcher
 *  Copyright (C) 2026 introllmortal <introllmortal@users.noreply.github.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "GpuSanityChecker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include "FileSystem.h"

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#if defined(_MSC_VER)
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "advapi32.lib")
#endif
#endif

#ifdef Q_OS_WIN
namespace {
static const wchar_t* kDirectXSubKey = L"Software\\Microsoft\\DirectX\\UserGpuPreferences";
static const wchar_t* kHighPerfValue = L"GpuPreference=2;";

bool writeRegistryPreference(const QString& filePath, bool remove)
{
    if (filePath.isEmpty()) {
        return false;
    }

    std::wstring wPath = QDir::toNativeSeparators(filePath).toStdWString();
    HKEY hKey = nullptr;

    LSTATUS status = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        kDirectXSubKey,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_READ | KEY_WRITE,
        nullptr,
        &hKey,
        nullptr
    );

    if (status != ERROR_SUCCESS || !hKey) {
        return false;
    }

    bool success = false;
    if (remove) {
        status = RegDeleteValueW(hKey, wPath.c_str());
        success = (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND);
    } else {
        status = RegSetValueExW(
            hKey,
            wPath.c_str(),
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(kHighPerfValue),
            static_cast<DWORD>((wcslen(kHighPerfValue) + 1) * sizeof(wchar_t))
        );
        success = (status == ERROR_SUCCESS);
    }

    RegCloseKey(hKey);
    return success;
}

bool queryRegistryPreference(const QString& filePath)
{
    if (filePath.isEmpty()) {
        return false;
    }

    std::wstring wPath = QDir::toNativeSeparators(filePath).toStdWString();
    HKEY hKey = nullptr;

    LSTATUS status = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        kDirectXSubKey,
        0,
        KEY_READ,
        &hKey
    );

    if (status != ERROR_SUCCESS || !hKey) {
        return false;
    }

    wchar_t buffer[256] = {0};
    DWORD bufferSize = sizeof(buffer);
    DWORD type = 0;

    status = RegQueryValueExW(
        hKey,
        wPath.c_str(),
        nullptr,
        &type,
        reinterpret_cast<LPBYTE>(buffer),
        &bufferSize
    );

    RegCloseKey(hKey);

    if (status == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ)) {
        QString val = QString::fromWCharArray(buffer);
        return val.contains("GpuPreference=2");
    }

    return false;
}
}  // namespace
#endif

bool GpuSanityChecker::isSupported()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

std::vector<GpuAdapterInfo> GpuSanityChecker::detectAdapters()
{
    std::vector<GpuAdapterInfo> adapters;
#ifdef Q_OS_WIN
    using Microsoft::WRL::ComPtr;

    ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) {
        return adapters;
    }

    UINT index = 0;
    ComPtr<IDXGIAdapter1> adapter;
    while (factory->EnumAdapters1(index, &adapter) != DXGI_ERROR_NOT_FOUND) {
        if (adapter) {
            DXGI_ADAPTER_DESC1 desc;
            if (SUCCEEDED(adapter->GetDesc1(&desc))) {
                GpuAdapterInfo info;
                info.name = QString::fromWCharArray(desc.Description);
                info.vendorId = desc.VendorId;
                info.deviceId = desc.DeviceId;
                info.dedicatedVideoMemory = desc.DedicatedVideoMemory;

                if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) || desc.VendorId == 0x1414) {
                    info.isSoftware = true;
                } else {
                    info.isSoftware = false;

                    if (desc.VendorId == 0x10DE) {
                        info.isDiscrete = true;
                        info.isIntegrated = false;
                    } else if (desc.VendorId == 0x8086) {
                        if (desc.DedicatedVideoMemory >= 2048ULL * 1024 * 1024) {
                            info.isDiscrete = true;
                            info.isIntegrated = false;
                        } else {
                            info.isDiscrete = false;
                            info.isIntegrated = true;
                        }
                    } else if (desc.VendorId == 0x1002) {
                        if (desc.DedicatedVideoMemory >= 2048ULL * 1024 * 1024) {
                            info.isDiscrete = true;
                            info.isIntegrated = false;
                        } else {
                            info.isDiscrete = false;
                            info.isIntegrated = true;
                        }
                    } else {
                        if (desc.DedicatedVideoMemory >= 2048ULL * 1024 * 1024) {
                            info.isDiscrete = true;
                            info.isIntegrated = false;
                        } else {
                            info.isDiscrete = false;
                            info.isIntegrated = true;
                        }
                    }

                    adapters.push_back(info);
                }
            }
        }
        ++index;
    }
#endif
    return adapters;
}

bool GpuSanityChecker::isDualGpu()
{
#ifdef Q_OS_WIN
    auto adapters = detectAdapters();
    bool hasDiscrete = false;
    bool hasIntegrated = false;
    for (const auto& a : adapters) {
        if (a.isDiscrete) {
            hasDiscrete = true;
        }
        if (a.isIntegrated) {
            hasIntegrated = true;
        }
    }
    return hasDiscrete && (hasIntegrated || adapters.size() > 1);
#else
    return false;
#endif
}

QStringList GpuSanityChecker::discreteGpuNames()
{
    QStringList names;
#ifdef Q_OS_WIN
    for (const auto& a : detectAdapters()) {
        if (a.isDiscrete) {
            names.append(a.name);
        }
    }
#endif
    return names;
}

QStringList GpuSanityChecker::integratedGpuNames()
{
    QStringList names;
#ifdef Q_OS_WIN
    for (const auto& a : detectAdapters()) {
        if (a.isIntegrated) {
            names.append(a.name);
        }
    }
#endif
    return names;
}

QString GpuSanityChecker::validateAndCanonicalizeJavaPath(const QString& javaPath)
{
    if (javaPath.trimmed().isEmpty()) {
        return QString();
    }

    QString candidate = javaPath.trimmed();
    QFileInfo fi(candidate);
    if (!fi.exists()) {
        QString resolved = FS::ResolveExecutable(candidate);
        if (!resolved.isEmpty()) {
            fi.setFile(resolved);
        }
    }

    if (!fi.exists() || !fi.isFile()) {
        return QString();
    }

    QString canonical = fi.canonicalFilePath();
    if (canonical.isEmpty()) {
        canonical = fi.absoluteFilePath();
    }

    QString fn = fi.fileName().toLower();
    if (fn != "javaw.exe" && fn != "java.exe") {
        return QString();
    }

    return QDir::toNativeSeparators(canonical);
}

bool GpuSanityChecker::isHighPerformanceEnforced(const QString& javaPath)
{
#ifdef Q_OS_WIN
    QString validPath = validateAndCanonicalizeJavaPath(javaPath);
    if (validPath.isEmpty()) {
        return false;
    }

    if (queryRegistryPreference(validPath)) {
        return true;
    }

    QFileInfo fi(validPath);
    QString dir = fi.absolutePath();
    QString siblingName = (fi.fileName().toLower() == "javaw.exe") ? "java.exe" : "javaw.exe";
    QString siblingPath = QDir::toNativeSeparators(dir + QDir::separator() + siblingName);
    if (QFile::exists(siblingPath) && queryRegistryPreference(siblingPath)) {
        return true;
    }

    return false;
#else
    Q_UNUSED(javaPath);
    return true;
#endif
}

bool GpuSanityChecker::setGpuPreference(const QString& javaPath)
{
#ifdef Q_OS_WIN
    QString validPath = validateAndCanonicalizeJavaPath(javaPath);
    if (validPath.isEmpty()) {
        return false;
    }

    bool ok = writeRegistryPreference(validPath, false);

    QFileInfo fi(validPath);
    QString dir = fi.absolutePath();
    QString siblingName = (fi.fileName().toLower() == "javaw.exe") ? "java.exe" : "javaw.exe";
    QString siblingPath = QDir::toNativeSeparators(dir + QDir::separator() + siblingName);
    if (QFile::exists(siblingPath)) {
        writeRegistryPreference(siblingPath, false);
    }

    return ok;
#else
    Q_UNUSED(javaPath);
    return true;
#endif
}

bool GpuSanityChecker::removeGpuPreference(const QString& javaPath)
{
#ifdef Q_OS_WIN
    QString validPath = validateAndCanonicalizeJavaPath(javaPath);
    if (validPath.isEmpty()) {
        return false;
    }

    bool ok = writeRegistryPreference(validPath, true);

    QFileInfo fi(validPath);
    QString dir = fi.absolutePath();
    QString siblingName = (fi.fileName().toLower() == "javaw.exe") ? "java.exe" : "javaw.exe";
    QString siblingPath = QDir::toNativeSeparators(dir + QDir::separator() + siblingName);
    if (QFile::exists(siblingPath)) {
        writeRegistryPreference(siblingPath, true);
    }

    return ok;
#else
    Q_UNUSED(javaPath);
    return true;
#endif
}
