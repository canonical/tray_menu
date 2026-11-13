import 'dart:async';

import 'package:flutter/material.dart';
import 'package:tray_menu/tray_menu.dart';

void main() {
  runApp(const MaterialApp(home: MyHomePage()));
}

class MyHomePage extends StatefulWidget {
  const MyHomePage({super.key});

  @override
  State<MyHomePage> createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {
  Future<void> initTrayMenu() async {
    await TrayMenu.instance.addLabel('aaa', label: 'aaa', callback: (key, _) {
      TrayMenu.instance.get<MenuItemSubmenu>('ccc')?.remove('aaa');
    });
    await TrayMenu.instance.addSeparator('sep');
    await TrayMenu.instance
        .addLabel('bbb', label: 'bbb', callback: (key, _) => debugPrint(key));
    await TrayMenu.instance
        .addSubmenu('ccc', label: 'submenu', before: 'sep')
        .then((menu) async {
      await menu.addLabel('aaa',
          label: 'aaa', callback: (key, _) => debugPrint(key));
      await menu.addLabel('ccc',
          label: 'ccc', callback: (key, _) => debugPrint(key));
      await menu.addLabel('bbb',
          label: 'bbb', before: 'ccc', callback: (key, _) => debugPrint(key));
    });
    await TrayMenu.instance.show('/home/andrei/icon.ico');
    Timer(const Duration(seconds: 10), () {
      TrayMenu.instance
          .addSubmenu('ddd', label: 'submenu 2', before: 'sep')
          .then((menu) async {
        await menu.addLabel('aaa',
            label: 'aaa', callback: (key, _) => debugPrint(key));
        await menu.addLabel('ccc',
            label: 'ccc', callback: (key, _) => debugPrint(key));
        await menu.addLabel('bbb',
            label: 'bbb', before: 'ccc', callback: (key, _) => debugPrint(key));
      });
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      floatingActionButton: FloatingActionButton(
        onPressed: initTrayMenu,
      ),
    );
  }
}
