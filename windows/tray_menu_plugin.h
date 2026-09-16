#ifndef FLUTTER_PLUGIN_TRAY_MENU_PLUGIN_H_
#define FLUTTER_PLUGIN_TRAY_MENU_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>

#include <memory>
#include <unordered_map>

namespace tray_menu {
class TrayMenuPlugin : public flutter::Plugin {
public:
    static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar);

    TrayMenuPlugin(flutter::PluginRegistrarWindows* registrar);
    virtual ~TrayMenuPlugin();

    TrayMenuPlugin(const TrayMenuPlugin&)            = delete;
    TrayMenuPlugin& operator=(const TrayMenuPlugin&) = delete;

    void HandleMethodCall(const flutter::MethodCall<flutter::EncodableValue>& method_call,
                            std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

    std::optional<LRESULT> window_proc_delegate(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    void init(const flutter::EncodableValue* args, flutter::MethodResult<>& result);
    void show_tray_icon(const flutter::EncodableValue* args, flutter::MethodResult<>& result);
    void add_menu_item(const flutter::EncodableValue* args, flutter::MethodResult<>& result);
    void remove_menu_item(const flutter::EncodableValue* args, flutter::MethodResult<>& result);
    void get_menu_item_label(const flutter::EncodableValue* args, flutter::MethodResult<>& result);
    void set_menu_item_label(const flutter::EncodableValue* args, flutter::MethodResult<>& result);
    void get_menu_item_enabled(const flutter::EncodableValue* args, flutter::MethodResult<>& result);
    void set_menu_item_enabled(const flutter::EncodableValue* args, flutter::MethodResult<>& result);
    void get_menu_item_checked(const flutter::EncodableValue* args, flutter::MethodResult<>& result);
    void set_menu_item_checked(const flutter::EncodableValue* args, flutter::MethodResult<>& result);

    flutter::PluginRegistrarWindows* registrar;
    std::unique_ptr<flutter::MethodChannel<>> channel{};
    UINT proc_delegate_id;
    NOTIFYICONDATA nid = {sizeof(NOTIFYICONDATA)};
    HMENU menu         = CreatePopupMenu();
    std::unordered_map<UINT, HMENU> parents;
    static HBITMAP unchecked_bitmap;
};
}// namespace tray_menu

#endif// FLUTTER_PLUGIN_TRAY_MENU_PLUGIN_H_
