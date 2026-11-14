part of 'tray_menu.dart';

/// An implementation of [TrayMenuPlatform] that uses method channels.
class MethodChannelTrayMenu extends TrayMenuPlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel('tray_menu');

  @override
  void setCallbackHandler(Future<dynamic> Function(MethodCall) callback) {
    methodChannel.setMethodCallHandler(callback);
  }

  @override
  Future<void> init() => methodChannel.invokeMethod('init');

  @override
  Future<void> show(String iconPath) =>
      methodChannel.invokeMethod('showTrayIcon', iconPath);

  @override
  // ignore: library_private_types_in_public_api
  Future<int> add(_MenuItem item, {int? submenu, int? before}) async {
    final handle = await methodChannel.invokeMethod<int>(
      'addMenuItem',
      {
        ...item.toMap(),
        if (submenu != null) 'submenu': submenu,
        if (before != null) 'before': before,
      },
    );
    return handle!;
  }

  @override
  Future<void> remove(int handle) {
    return methodChannel.invokeMethod('removeMenuItem', handle);
  }

  @override
  Future<String> getMenuItemLabel(int handle) async {
    final label = await methodChannel.invokeMethod<String>(
      'getMenuItemLabel',
      handle,
    );
    return label!;
  }

  @override
  Future<void> setMenuItemLabel(int handle, String label) {
    return methodChannel.invokeMethod('setMenuItemLabel', {
      'handle': handle,
      'label': label,
    });
  }

  @override
  Future<bool> getMenuItemEnabled(int handle) async {
    final enabled = await methodChannel.invokeMethod<bool>(
      'getMenuItemEnabled',
      handle,
    );
    return enabled!;
  }

  @override
  Future<void> setMenuItemEnabled(int handle, bool enabled) {
    return methodChannel.invokeMethod('setMenuItemEnabled', {
      'handle': handle,
      'enabled': enabled,
    });
  }

  @override
  Future<bool> getMenuItemChecked(int handle) async {
    final enabled = await methodChannel.invokeMethod<bool>(
      'getMenuItemChecked',
      handle,
    );
    return enabled!;
  }

  @override
  Future<void> setMenuItemChecked(int handle, bool checked) {
    return methodChannel.invokeMethod('setMenuItemChecked', {
      'handle': handle,
      'checked': checked,
    });
  }
}
