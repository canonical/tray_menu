#include "tray_menu_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>
#include <windowsx.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <functional>
#include <unordered_map>

namespace tray_menu {
HBITMAP GenerateUncheckedBitmap() {
    const auto width                    = GetSystemMetrics(SM_CXMENUCHECK) + 1;
    const auto height                   = GetSystemMetrics(SM_CYMENUCHECK) + 1;
    const auto device_context           = GetDC(GetDesktopWindow());
    const auto in_memory_device_context = CreateCompatibleDC(device_context);
    const auto bitmap                   = CreateCompatibleBitmap(device_context, width, height);
    const auto old_bitmap               = (HBITMAP) SelectObject(in_memory_device_context, bitmap);

    RECT rect              = {0, 0, width + 1, height + 1};
    const auto fill_brush  = CreateSolidBrush(RGB(220, 220, 220));
    const auto frame_brush = CreateSolidBrush(RGB(200, 200, 200));
    FillRect(in_memory_device_context, &rect, fill_brush);
    FrameRect(in_memory_device_context, &rect, frame_brush);

    SelectObject(in_memory_device_context, old_bitmap);
    DeleteBrush(fill_brush);
    DeleteBrush(frame_brush);
    DeleteDC(in_memory_device_context);

    return bitmap;
}

// static
void TrayMenuPlugin::RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar) {
    auto channel = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), "tray_menu", &flutter::StandardMethodCodec::GetInstance());

    auto plugin = std::make_unique<TrayMenuPlugin>(registrar);
    channel->SetMethodCallHandler([plugin_pointer = plugin.get()](const auto& call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
    });
    plugin->channel = std::move(channel);
    registrar->AddPlugin(std::move(plugin));
}

HBITMAP TrayMenuPlugin::unchecked_bitmap = GenerateUncheckedBitmap();

TrayMenuPlugin::TrayMenuPlugin(flutter::PluginRegistrarWindows* registrar) : registrar{registrar} {
    using namespace std::placeholders;
    proc_delegate_id = registrar->RegisterTopLevelWindowProcDelegate(
            std::bind(&TrayMenuPlugin::window_proc_delegate, this, _1, _2, _3, _4));

    nid.uVersion         = NOTIFYICON_VERSION_4;
    nid.uCallbackMessage = RegisterWindowMessage(L"SystemTrayNotify");
    nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
}

TrayMenuPlugin::~TrayMenuPlugin() {
    registrar->UnregisterTopLevelWindowProcDelegate(proc_delegate_id);
    DestroyIcon(nid.hIcon);
    Shell_NotifyIcon(NIM_DELETE, &nid);
    DestroyMenu(menu);
    DeleteBitmap(unchecked_bitmap);
}

void TrayMenuPlugin::HandleMethodCall(const flutter::MethodCall<flutter::EncodableValue>& method_call,
                                      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    using MethodHandler = void (TrayMenuPlugin::*)(const flutter::EncodableValue*, flutter::MethodResult<>&);
    static const std::unordered_map<std::string, MethodHandler> handlers = {
            {"init", &TrayMenuPlugin::init},
            {"showTrayIcon", &TrayMenuPlugin::show_tray_icon},
            {"addMenuItem", &TrayMenuPlugin::add_menu_item},
            {"removeMenuItem", &TrayMenuPlugin::remove_menu_item},
            {"getMenuItemLabel", &TrayMenuPlugin::get_menu_item_label},
            {"setMenuItemLabel", &TrayMenuPlugin::set_menu_item_label},
            {"getMenuItemEnabled", &TrayMenuPlugin::get_menu_item_enabled},
            {"setMenuItemEnabled", &TrayMenuPlugin::set_menu_item_enabled},
            {"getMenuItemChecked", &TrayMenuPlugin::get_menu_item_checked},
            {"setMenuItemChecked", &TrayMenuPlugin::set_menu_item_checked},
    };

    if (const auto it = handlers.find(method_call.method_name()); it != handlers.end()) {
        (this->*it->second)(method_call.arguments(), *result);
    } else {
        result->NotImplemented();
    }
}

std::optional<LRESULT> TrayMenuPlugin::window_proc_delegate(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message != nid.uCallbackMessage || LOWORD(lparam) != WM_LBUTTONUP) {
        return std::nullopt;
    }

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    const int32_t handle = TrackPopupMenuEx(menu, TPM_LEFTBUTTON | TPM_RETURNCMD, pt.x, pt.y, nid.hWnd, nullptr);
    if (!handle) {
        return std::nullopt;
    }
    MENUITEMINFO item = {sizeof(MENUITEMINFO)};
    item.fMask        = MIIM_CHECKMARKS;
    GetMenuItemInfo(menu, handle, false, &item);
    if (item.hbmpUnchecked == unchecked_bitmap) {
        const auto unchecked = !(MFS_CHECKED & GetMenuState(menu, handle, MF_BYCOMMAND));
        CheckMenuItem(menu, handle, unchecked ? MF_CHECKED : MF_UNCHECKED);
    }
    channel->InvokeMethod("itemCallback", std::make_unique<flutter::EncodableValue>(handle));

    return std::nullopt;
}

std::wstring ConvertUft8ToUtf16(const std::string& utf8) {
    auto size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring utf16(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, utf16.data(), size);
    return utf16;
}

std::string ConvertUtf16ToUtf8(const std::wstring& utf16) {
    auto size = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, nullptr, 0, nullptr, false);
    std::string utf8(size, '0');
    WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, utf8.data(), size, nullptr, false);
    return utf8;
}

void TrayMenuPlugin::init(const flutter::EncodableValue* args, flutter::MethodResult<>& result) {
    DestroyIcon(nid.hIcon);
    Shell_NotifyIcon(NIM_DELETE, &nid);
    DestroyMenu(menu);
    parents.clear();

    menu = CreatePopupMenu();
    result.Success();
}

void TrayMenuPlugin::show_tray_icon(const flutter::EncodableValue* args, flutter::MethodResult<>& result) {
    const auto& icon_path = ConvertUft8ToUtf16(std::get<std::string>(*args));
    nid.hIcon             = static_cast<HICON>(
            LoadImage(nullptr, icon_path.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
    nid.hWnd = GetAncestor(registrar->GetView()->GetNativeWindow(), GA_ROOT);
    Shell_NotifyIcon(NIM_ADD, &nid);
    result.Success();
}

HMENU get_parent_or_default(UINT handle, const std::unordered_map<UINT, HMENU> parents, HMENU default_value) {
    auto it = parents.find(handle);
    return it != parents.end() ? it->second : default_value;
}

MENUITEMINFO create_label_menu_item(const flutter::EncodableMap& args) {
    auto label         = ConvertUft8ToUtf16(std::get<std::string>(args.at(flutter::EncodableValue{"label"})));
    const auto enabled = std::get<bool>(args.at(flutter::EncodableValue{"enabled"}));
    MENUITEMINFO item  = {sizeof(MENUITEMINFO)};
    item.fMask |= MIIM_ID | MIIM_STATE | MIIM_STRING;
    item.fState     = enabled ? MFS_ENABLED : MFS_DISABLED;
    item.dwTypeData = _wcsdup(label.c_str());
    item.cch        = static_cast<UINT>(label.length());
    return item;
}

MENUITEMINFO create_separator_menu_item(const flutter::EncodableMap&) {
    MENUITEMINFO item = {sizeof(MENUITEMINFO)};
    item.fMask |= MIIM_ID | MIIM_STATE | MIIM_FTYPE;
    item.fType = MFT_SEPARATOR;
    return item;
}

MENUITEMINFO create_checkbox_menu_item(const flutter::EncodableMap& args) {
    auto label         = ConvertUft8ToUtf16(std::get<std::string>(args.at(flutter::EncodableValue{"label"})));
    const auto enabled = std::get<bool>(args.at(flutter::EncodableValue{"enabled"}));
    const auto checked = std::get<bool>(args.at(flutter::EncodableValue{"checked"}));
    MENUITEMINFO item  = {sizeof(MENUITEMINFO)};
    item.fMask |= MIIM_ID | MIIM_STATE | MIIM_STRING | MIIM_CHECKMARKS;
    item.fState        = (enabled ? MFS_ENABLED : MFS_DISABLED) | (checked ? MFS_CHECKED : MFS_UNCHECKED);
    item.dwTypeData    = _wcsdup(label.c_str());
    item.cch           = static_cast<UINT>(label.length());
    item.hbmpUnchecked = TrayMenuPlugin::unchecked_bitmap;
    return item;
}

MENUITEMINFO create_submenu_menu_item(const flutter::EncodableMap& args) {
    MENUITEMINFO item  = {sizeof(MENUITEMINFO)};
    auto label         = ConvertUft8ToUtf16(std::get<std::string>(args.at(flutter::EncodableValue{"label"})));
    const auto enabled = std::get<bool>(args.at(flutter::EncodableValue{"enabled"}));
    item.fMask |= MIIM_ID | MIIM_STATE | MIIM_STRING | MIIM_SUBMENU;
    item.fState     = enabled ? MFS_ENABLED : MFS_DISABLED;
    item.dwTypeData = _wcsdup(label.c_str());
    item.cch        = static_cast<UINT>(label.length());
    item.hSubMenu   = CreatePopupMenu();
    return item;
}

void TrayMenuPlugin::add_menu_item(const flutter::EncodableValue* args, flutter::MethodResult<>& result) {
    static UINT next_handle   = 1;
    using MenuItemConstructor = MENUITEMINFO (*)(const flutter::EncodableMap&);
    static const std::unordered_map<std::string, MenuItemConstructor> menu_item_constructors = {
            {"_MenuItemLabel", create_label_menu_item},
            {"_MenuItemSeparator", create_separator_menu_item},
            {"_MenuItemCheckbox", create_checkbox_menu_item},
            {"_MenuItemSubmenu", create_submenu_menu_item},
    };

    const auto& map                  = std::get<flutter::EncodableMap>(*args);
    const auto& type                 = std::get<std::string>(map.at(flutter::EncodableValue{"type"}));
    const auto menu_item_constructor = menu_item_constructors.at(type);

    auto parent_menu = menu;

    if (const auto submenu_value = map.find(flutter::EncodableValue{"submenu"}); submenu_value != map.end()) {
        const auto submenu_item_handle = static_cast<UINT>(std::get<int32_t>(submenu_value->second));
        const auto parent              = get_parent_or_default(submenu_item_handle, parents, menu);

        MENUITEMINFO item              = {sizeof(MENUITEMINFO)};
        item.fMask                     = MIIM_SUBMENU;
        GetMenuItemInfo(parent, submenu_item_handle, false, &item);
        parent_menu = item.hSubMenu;
    }

    const auto before_value = map.find(flutter::EncodableValue{"before"});
    const auto before       = before_value != map.end() ? std::get<int32_t>(before_value->second) : -1;

    auto item = menu_item_constructor(map);
    item.wID  = next_handle++;
    InsertMenuItem(parent_menu, static_cast<UINT>(before), false, &item);
    free(item.dwTypeData);

    parents.emplace(item.wID, parent_menu);

    result.Success(flutter::EncodableValue(static_cast<int32_t>(item.wID)));
}

void TrayMenuPlugin::remove_menu_item(const flutter::EncodableValue* args, flutter::MethodResult<>& result) {
    const auto handle = static_cast<UINT>(std::get<int32_t>(*args));

    auto it = parents.find(handle);
    if (it != parents.end()) {
        DeleteMenu(it->second, handle, MF_BYCOMMAND);
        parents.erase(it);
    }
    else {
        DeleteMenu(menu, handle, MF_BYCOMMAND);
    }

    result.Success();
}

void TrayMenuPlugin::get_menu_item_label(const flutter::EncodableValue* args, flutter::MethodResult<>& result) {
    const auto handle = std::get<int32_t>(*args);
    const auto parent = get_parent_or_default(handle, parents, menu);

    MENUITEMINFO item = {sizeof(MENUITEMINFO)};
    item.fMask        = MIIM_STRING;
    item.dwTypeData   = nullptr;
    GetMenuItemInfo(parent, handle, false, &item);
    std::wstring buffer(item.cch++, L'0');
    item.dwTypeData = buffer.data();
    GetMenuItemInfo(parent, handle, false, &item);
    result.Success(flutter::EncodableValue{ConvertUtf16ToUtf8({item.dwTypeData, item.cch})});
}

void TrayMenuPlugin::set_menu_item_label(const flutter::EncodableValue* args, flutter::MethodResult<>& result) {
    const auto& map   = std::get<flutter::EncodableMap>(*args);
    const auto handle = std::get<int32_t>(map.at(flutter::EncodableValue{"handle"}));
    const auto parent = get_parent_or_default(handle, parents, menu);

    auto label        = ConvertUft8ToUtf16(std::get<std::string>(map.at(flutter::EncodableValue{"label"})));
    MENUITEMINFO item = {sizeof(MENUITEMINFO)};
    item.fMask        = MIIM_STRING;
    item.dwTypeData   = label.data();
    item.cch          = static_cast<UINT>(label.length());
    SetMenuItemInfo(parent, handle, false, &item);
    result.Success();
}

void TrayMenuPlugin::get_menu_item_enabled(const flutter::EncodableValue* args, flutter::MethodResult<>& result) {
    const auto handle = std::get<int32_t>(*args);
    const auto parent = get_parent_or_default(handle, parents, menu);

    const auto enabled = !(MFS_DISABLED & GetMenuState(parent, handle, MF_BYCOMMAND));
    result.Success(flutter::EncodableValue{enabled});
}

void TrayMenuPlugin::set_menu_item_enabled(const flutter::EncodableValue* args, flutter::MethodResult<>& result) {
    const auto& map   = std::get<flutter::EncodableMap>(*args);
    const auto handle = std::get<int32_t>(map.at(flutter::EncodableValue{"handle"}));
    const auto parent = get_parent_or_default(handle, parents, menu);

    const auto enabled = std::get<bool>(map.at(flutter::EncodableValue{"enabled"}));
    EnableMenuItem(parent, handle, enabled ? MF_ENABLED : MF_DISABLED);
    result.Success();
}

void TrayMenuPlugin::get_menu_item_checked(const flutter::EncodableValue* args, flutter::MethodResult<>& result) {
    const auto handle = std::get<int32_t>(*args);
    const auto parent = get_parent_or_default(handle, parents, menu);

    const auto checked = !!(MFS_CHECKED & GetMenuState(parent, handle, MF_BYCOMMAND));
    result.Success(flutter::EncodableValue{checked});
}

void TrayMenuPlugin::set_menu_item_checked(const flutter::EncodableValue* args, flutter::MethodResult<>& result) {
    const auto& map   = std::get<flutter::EncodableMap>(*args);
    const auto handle = std::get<int32_t>(map.at(flutter::EncodableValue{"handle"}));
    const auto parent = get_parent_or_default(handle, parents, menu);

    const auto checked = std::get<bool>(map.at(flutter::EncodableValue{"checked"}));
    CheckMenuItem(parent, handle, checked ? MF_CHECKED : MF_UNCHECKED);
    result.Success();
}
}// namespace tray_menu
