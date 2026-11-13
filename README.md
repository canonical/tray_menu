# tray_menu

A Flutter plugin for system tray icons and menus on Linux, macOS and Windows.

## Usage

```dart
import 'package:tray_menu/tray_menu.dart';

Future<void> setUpTray() async {
  final tray = TrayMenu.instance;

  await tray.addLabel('open', label: 'Open', callback: (key, item) {});
  await tray.addSeparator('sep');
  final submenu = await tray.addSubmenu('more', label: 'More', before: 'sep');
  await submenu.addCheckbox('auto', label: 'Autostart', checked: true);

  await tray.show('/path/to/icon.png');
}
```

Each menu item is identified by a unique key, which can be used to look it up
(`get<T>(key)`), remove it (`remove(key)`) or insert other items before it
(`before:`).

See [example/](example/) for a runnable demo.
