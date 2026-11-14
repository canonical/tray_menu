#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html.
# Run `pod lib lint tray_menu.podspec` to validate before publishing.
#
Pod::Spec.new do |s|
  s.name             = 'tray_menu'
  s.version          = '0.0.1'
  s.summary          = 'System tray icons and menus for Flutter desktop apps.'
  s.description      = <<-DESC
A Flutter plugin for system tray icons and menus on Linux, macOS and Windows.
                       DESC
  s.homepage         = 'https://github.com/canonical/tray_menu'
  s.license          = { :file => '../LICENSE' }
  s.author           = 'Canonical Ltd.'

  s.source           = { :path => '.' }
  s.source_files     = 'Classes/**/*'
  s.dependency 'FlutterMacOS'

  s.platform = :osx, '10.11'
  s.pod_target_xcconfig = { 'DEFINES_MODULE' => 'YES' }
  s.swift_version = '5.0'
end
