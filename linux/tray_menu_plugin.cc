#include "include/tray_menu/tray_menu_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>
#include <gtkmm.h>
#include <libayatana-appindicator/app-indicator.h>

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#define TRAY_MENU_PLUGIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), tray_menu_plugin_get_type(), TrayMenuPlugin))

struct IndexedMenu : public Gtk::Menu {
    void add_item(int64_t handle, std::unique_ptr<Gtk::MenuItem> item, int64_t before = -1) {
        if (before >= 0) {
            const auto& children = get_children();
            const auto& child    = std::find(children.begin(), children.end(), items.at(before).get());
            const auto position  = static_cast<int>(child - children.begin());
            insert(*item, position);
        } else {
            append(*item);
        }
        item->show();
        items.insert({handle, std::move(item)});
    }

    template<typename T = Gtk::MenuItem, typename = std::enable_if_t<std::is_base_of<Gtk::MenuItem, T>::value>>
    T* get_item(int64_t handle) {
        auto it = items.find(handle);
        if (it != items.end()) {
            return dynamic_cast<T*>(it->second.get());
        }
        for (const auto& it2 : items) {
            const auto& item = it2.second;
            auto submenu     = dynamic_cast<IndexedMenu*>(item->get_submenu());
            if (submenu) {
                auto foundItem = submenu->get_item<T>(handle);
                if (foundItem) {
                    return foundItem;
                }
            }
        }
        return nullptr;
    }

    bool remove_item(int64_t handle) {
        if (items.erase(handle)) {
            return true;
        }
        for (const auto& it : items) {
            const auto& item = it.second;
            auto submenu     = dynamic_cast<IndexedMenu*>(item->get_submenu());
            if (submenu) {
                if (submenu->remove_item(handle)) {
                    return true;
                }
            }
        }
        return false;
    }

private:
    std::unordered_map<int64_t, std::unique_ptr<Gtk::MenuItem>> items{};
};

struct MenuItemSubmenu : public Gtk::MenuItem {
    IndexedMenu menu{};
};

struct _TrayMenuPlugin {
    GObject parent_instance;
    FlMethodChannel* channel;
    AppIndicator* app_indicator;
    IndexedMenu menu;

    FlMethodResponse* init(FlValue* args);

    FlMethodResponse* show_tray_icon(FlValue* args);

    FlMethodResponse* add_menu_item(FlValue* args);

    FlMethodResponse* remove_menu_item(FlValue* args);

    FlMethodResponse* get_menu_item_label(FlValue* args);

    FlMethodResponse* set_menu_item_label(FlValue* args);

    FlMethodResponse* get_menu_item_enabled(FlValue* args);

    FlMethodResponse* set_menu_item_enabled(FlValue* args);

    FlMethodResponse* get_menu_item_checked(FlValue* args);

    FlMethodResponse* set_menu_item_checked(FlValue* args);
};

G_DEFINE_TYPE(TrayMenuPlugin, tray_menu_plugin, g_object_get_type())

FlMethodResponse* TrayMenuPlugin::init(FlValue* args) {
    g_clear_object(&app_indicator);
    menu = IndexedMenu{};
    return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

FlMethodResponse* TrayMenuPlugin::show_tray_icon(FlValue* args) {
    if (app_indicator) {
        return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
    }
    const auto icon = fl_value_get_string(args);
    app_indicator   = app_indicator_new("tray-icon", icon, APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    app_indicator_set_status(app_indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_menu(app_indicator, menu.gobj());
    return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

std::unique_ptr<Gtk::MenuItem> create_label_menu_item(FlValue* args) {
    const gchar* label = fl_value_get_string(fl_value_lookup_string(args, "label"));
    const bool enabled = fl_value_get_bool(fl_value_lookup_string(args, "enabled"));
    auto item          = std::make_unique<Gtk::MenuItem>(label);
    item->set_sensitive(enabled);
    return item;
}

std::unique_ptr<Gtk::MenuItem> create_separator_menu_item(FlValue*) {
    return std::make_unique<Gtk::SeparatorMenuItem>();
}

std::unique_ptr<Gtk::MenuItem> create_checkbox_menu_item(FlValue* args) {
    const gchar* label = fl_value_get_string(fl_value_lookup_string(args, "label"));
    const bool enabled = fl_value_get_bool(fl_value_lookup_string(args, "enabled"));
    const bool checked = fl_value_get_bool(fl_value_lookup_string(args, "checked"));
    auto item          = std::make_unique<Gtk::CheckMenuItem>(label);
    item->set_sensitive(enabled);
    item->set_active(checked);
    return item;
}

std::unique_ptr<Gtk::MenuItem> create_submenu_menu_item(FlValue* args) {
    const gchar* label = fl_value_get_string(fl_value_lookup_string(args, "label"));
    const bool enabled = fl_value_get_bool(fl_value_lookup_string(args, "enabled"));
    auto item          = std::make_unique<Gtk::MenuItem>(label);
    auto submenu       = Gtk::make_managed<IndexedMenu>();
    item->set_submenu(*submenu);
    item->set_sensitive(enabled);
    return item;
}

FlMethodResponse* TrayMenuPlugin::add_menu_item(FlValue* args) {
    static int64_t next_handle = 0;

    static const std::unordered_map<std::string, std::unique_ptr<Gtk::MenuItem> (*)(FlValue*)> menu_item_constructors =
            {
                    {"_MenuItemLabel", create_label_menu_item},
                    {"_MenuItemSeparator", create_separator_menu_item},
                    {"_MenuItemCheckbox", create_checkbox_menu_item},
                    {"_MenuItemSubmenu", create_submenu_menu_item},
            };

    const gchar* type                = fl_value_get_string(fl_value_lookup_string(args, "type"));
    const auto menu_item_constructor = menu_item_constructors.at(type);

    auto parent_menu         = &menu;
    const auto submenu_value = fl_value_lookup_string(args, "submenu");
    if (submenu_value) {
        const int64_t submenu_handle = fl_value_get_int(submenu_value);
        auto submenu_item            = menu.get_item(submenu_handle);
        if (!submenu_item || !submenu_item->has_submenu()) {
            return FL_METHOD_RESPONSE(fl_method_error_response_new("Invalid handle", nullptr, nullptr));
        }
        parent_menu = dynamic_cast<IndexedMenu*>(submenu_item->get_submenu());
    }

    const auto before_value = fl_value_lookup_string(args, "before");
    const auto before       = before_value ? fl_value_get_int(before_value) : -1;

    const auto handle = next_handle++;
    auto item         = menu_item_constructor(args);
    item->signal_activate().connect([=] {
        fl_method_channel_invoke_method(channel, "itemCallback", fl_value_new_int(handle), nullptr, nullptr, nullptr);
    });
    parent_menu->add_item(handle, std::move(item), before);

    g_autoptr(FlValue) result = fl_value_new_int(handle);
    return FL_METHOD_RESPONSE(fl_method_success_response_new(result));
}

FlMethodResponse* TrayMenuPlugin::remove_menu_item(FlValue* args) {
    const int64_t handle = fl_value_get_int(args);
    menu.remove_item(handle);
    return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

FlMethodResponse* TrayMenuPlugin::get_menu_item_label(FlValue* args) {
    const int64_t handle = fl_value_get_int(args);
    auto item            = menu.get_item(handle);
    if (!item) {
        return FL_METHOD_RESPONSE(fl_method_error_response_new("Invalid handle", nullptr, nullptr));
    }
    const auto label          = item->get_label();
    g_autoptr(FlValue) result = fl_value_new_string(label.data());
    return FL_METHOD_RESPONSE(fl_method_success_response_new(result));
}

FlMethodResponse* TrayMenuPlugin::set_menu_item_label(FlValue* args) {
    const int64_t handle = fl_value_get_int(fl_value_lookup_string(args, "handle"));
    const gchar* label   = fl_value_get_string(fl_value_lookup_string(args, "label"));
    auto item            = menu.get_item(handle);
    if (!item) {
        return FL_METHOD_RESPONSE(fl_method_error_response_new("Invalid handle", nullptr, nullptr));
    }
    item->set_label(label);
    return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

FlMethodResponse* TrayMenuPlugin::get_menu_item_enabled(FlValue* args) {
    const int64_t handle = fl_value_get_int(args);
    auto item            = menu.get_item(handle);
    if (!item) {
        return FL_METHOD_RESPONSE(fl_method_error_response_new("Invalid handle", nullptr, nullptr));
    }
    const auto enabled        = item->get_sensitive();
    g_autoptr(FlValue) result = fl_value_new_bool(enabled);
    return FL_METHOD_RESPONSE(fl_method_success_response_new(result));
}

FlMethodResponse* TrayMenuPlugin::set_menu_item_enabled(FlValue* args) {
    const int64_t handle = fl_value_get_int(fl_value_lookup_string(args, "handle"));
    const bool enabled   = fl_value_get_bool(fl_value_lookup_string(args, "enabled"));
    auto item            = menu.get_item(handle);
    if (!item) {
        return FL_METHOD_RESPONSE(fl_method_error_response_new("Invalid handle", nullptr, nullptr));
    }
    item->set_sensitive(enabled);
    return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

FlMethodResponse* TrayMenuPlugin::get_menu_item_checked(FlValue* args) {
    const int64_t handle = fl_value_get_int(args);
    auto item            = menu.get_item<Gtk::CheckMenuItem>(handle);
    if (!item) {
        return FL_METHOD_RESPONSE(fl_method_error_response_new("Invalid handle", nullptr, nullptr));
    }
    const auto checked        = item->get_active();
    g_autoptr(FlValue) result = fl_value_new_bool(checked);
    return FL_METHOD_RESPONSE(fl_method_success_response_new(result));
}

FlMethodResponse* TrayMenuPlugin::set_menu_item_checked(FlValue* args) {
    const int64_t handle = fl_value_get_int(fl_value_lookup_string(args, "handle"));
    const bool checked   = fl_value_get_bool(fl_value_lookup_string(args, "checked"));
    auto item            = menu.get_item<Gtk::CheckMenuItem>(handle);
    if (!item) {
        return FL_METHOD_RESPONSE(fl_method_error_response_new("Invalid handle", nullptr, nullptr));
    }
    item->set_active(checked);
    return FL_METHOD_RESPONSE(fl_method_success_response_new(nullptr));
}

static void tray_menu_plugin_handle_method_call(TrayMenuPlugin* self, FlMethodCall* method_call) {
    static const std::unordered_map<std::string, FlMethodResponse* (TrayMenuPlugin::*) (FlValue*)> handlers = {
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

    auto it = handlers.find(fl_method_call_get_name(method_call));

    g_autoptr(FlMethodResponse) response = it != handlers.end()
                                                   ? (self->*it->second)(fl_method_call_get_args(method_call))
                                                   : FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());

    fl_method_call_respond(method_call, response, nullptr);
}

static void tray_menu_plugin_dispose(GObject* object) {
    TrayMenuPlugin* self = TRAY_MENU_PLUGIN(object);
    G_OBJECT_CLASS(tray_menu_plugin_parent_class)->dispose(object);
    g_clear_object(&self->channel);
}

static void tray_menu_plugin_class_init(TrayMenuPluginClass* klass) {
    G_OBJECT_CLASS(klass)->dispose = tray_menu_plugin_dispose;
}

static void tray_menu_plugin_init(TrayMenuPlugin* self) {
    new Gtk::Main();
    Glib::init();
    new (&self->menu) IndexedMenu{};
}

static void method_call_cb(FlMethodChannel*, FlMethodCall* method_call, gpointer user_data) {
    tray_menu_plugin_handle_method_call(TRAY_MENU_PLUGIN(user_data), method_call);
}

void tray_menu_plugin_register_with_registrar(FlPluginRegistrar* registrar) {
    TrayMenuPlugin* plugin = TRAY_MENU_PLUGIN(g_object_new(tray_menu_plugin_get_type(), nullptr));

    g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();
    plugin->channel =
            fl_method_channel_new(fl_plugin_registrar_get_messenger(registrar), "tray_menu", FL_METHOD_CODEC(codec));
    fl_method_channel_set_method_call_handler(plugin->channel, method_call_cb, g_object_ref(plugin), g_object_unref);

    g_object_unref(plugin);
}
