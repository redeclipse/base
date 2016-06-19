#!/bin/sh
while [ -n "$1" ]; do
 case "$1" in
  clean)
   xcodebuild -project src/xcode/redeclipse.xcodeproj clean -configuration Release
   ;;
  all)
   xcodebuild -project src/xcode/redeclipse.xcodeproj -configuration Release -alltargets
   ;;
  install)
   cp -v src/xcode/build/Release/redeclipse.app/Contents/MacOS/redeclipse bin/redeclipse.app/Contents/MacOS/redeclipse_universal
   chmod +x bin/redeclipse.app/Contents/MacOS/redeclipse_universal
   ;;
 esac
 shift
done
