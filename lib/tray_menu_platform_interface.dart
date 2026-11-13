part of 'tray_menu.dart';

abstract class TrayMenuPlatform extends PlatformInterface {
  /// Constructs a TrayMenuPlatform.
  TrayMenuPlatform() : super(token: _token);

  static final Object _token = Object();

  static TrayMenuPlatform _instance = MethodChannelTrayMenu();

  /// The default instance of [TrayMenuPlatform] to use.
  ///
  /// Defaults to [MethodChannelTrayMenu].
  static TrayMenuPlatform get instance => _instance;

  /// Platform-specific implementations should set this with their own
  /// platform-specific class that extends [TrayMenuPlatform] when
  /// they register themselves.
  static set instance(TrayMenuPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  void setCallbackHandler(Future<dynamic> Function(MethodCall) callback) =>
      throw UnimplementedError();

  Future<void> init() => throw UnimplementedError();

  Future<void> show(String iconPath) => throw UnimplementedError();

  Future<int> add(_MenuItem item, {int? submenu, int? before}) =>
      throw UnimplementedError();

  Future<void> remove(int handle) => throw UnimplementedError();

  Future<String> getMenuItemLabel(int handle) => throw UnimplementedError();

  Future<void> setMenuItemLabel(int handle, String label) =>
      throw UnimplementedError();

  Future<bool> getMenuItemEnabled(int handle) => throw UnimplementedError();

  Future<void> setMenuItemEnabled(int handle, bool enabled) =>
      throw UnimplementedError();

  Future<bool> getMenuItemChecked(int handle) => throw UnimplementedError();

  Future<void> setMenuItemChecked(int handle, bool checked) =>
      throw UnimplementedError();
}
