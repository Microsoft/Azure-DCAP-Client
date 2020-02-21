#include "evtx_logging.h"

#define DCAP_EVTX_KEY "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\dcap_quoteprov"
#define DCAP_DLL_NAME "dcap_quoteprov"

#ifndef MAX_PATH
#define MAX_PATH 2048
#endif

extern HMODULE moduleHandle;

DWORD GetModuleFullName(std::string &moduleName)
{
    char buffer[MAX_PATH];

    DWORD copied = GetModuleFileNameA(moduleHandle, buffer, MAX_PATH);
    if (FAILED(copied))
    {
        return GetLastError();
    }

    moduleName = std::string(buffer);
    return copied;
}

DWORD check_install_event_log_source()
{
    const std::string key_path(DCAP_EVTX_KEY);
    wil::unique_hkey key;

    DWORD last_error = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        key_path.c_str(),
        0,
        KEY_READ,
        key.addressof());

    if (FAILED(last_error))
    {
        last_error = RegCreateKeyExA(
            HKEY_LOCAL_MACHINE,
            key_path.c_str(),
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE,
            NULL,
            key.addressof(),
            NULL);

        if (SUCCEEDED(last_error))
        {
            std::string fullPath;
            DWORD result = GetModuleFullName(fullPath);
            if (SUCCEEDED(result))
            {
                const DWORD types_supported = EVENTLOG_ERROR_TYPE |
                                              EVENTLOG_WARNING_TYPE |
                                              EVENTLOG_INFORMATION_TYPE;
                if (fullPath.length() <= MAXDWORD)
                {
                    last_error = RegSetValueExA(
                        key.get(),
                        "EventMessageFile",
                        0,
                        REG_SZ,
                        (BYTE*)fullPath.c_str(),
                        (DWORD)fullPath.length());

                    if (last_error == ERROR_SUCCESS)
                    {
                        last_error = RegSetValueExA(
                            key.get(),
                            "TypesSupported",
                            0,
                            REG_DWORD,
                            (LPBYTE)&types_supported,
                            sizeof(types_supported));
                    }
                }
                else
                {
                    return ERROR_BAD_LENGTH;
                }
            }
        }
    }

    DWORD status = RegCloseKey(key.get());
    if (SUCCEEDED(last_error))
    {
        return status;
    }

    return last_error;
}

DWORD uninstall_event_log_source()
{
    const std::string key_path(DCAP_EVTX_KEY);

    return RegDeleteKeyA(HKEY_LOCAL_MACHINE, key_path.c_str());
}

bool log_event_log_message(
    const std::string& a_msg,
    const WORD a_type)
{
    DWORD event_id;
    const std::string a_name(DCAP_DLL_NAME);
    bool success = false;

    switch (a_type)
    {
        case EVENTLOG_ERROR_TYPE:
            event_id = MSG_ERROR_1;
            break;
        case EVENTLOG_WARNING_TYPE:
            event_id = MSG_WARNING_1;
            break;
        case EVENTLOG_INFORMATION_TYPE:
            event_id = MSG_INFO_1;
            break;
        default:
            event_id = MSG_INFO_1;
            break;
    }

    HANDLE h_event_log = RegisterEventSourceA(0, a_name.c_str());

    if (h_event_log)
    {
        std::wstring a_w_msg(a_msg.begin(), a_msg.end());
        LPCWSTR message = a_w_msg.c_str();

        if (ReportEvent(h_event_log, a_type, 0, event_id, 0, 1, 0, &message, 0))
        {
            success = true;
        }

        DeregisterEventSource(h_event_log);
    }

    return success;
}
