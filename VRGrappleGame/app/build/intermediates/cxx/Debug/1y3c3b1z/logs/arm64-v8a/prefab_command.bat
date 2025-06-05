@echo off
"C:\\Program Files\\Android\\Android Studio\\jbr\\bin\\java" ^
  --class-path ^
  "C:\\Users\\Sawyer\\.gradle\\caches\\modules-2\\files-2.1\\com.google.prefab\\cli\\2.0.0\\f2702b5ca13df54e3ca92f29d6b403fb6285d8df\\cli-2.0.0-all.jar" ^
  com.google.prefab.cli.AppKt ^
  --build-system ^
  cmake ^
  --platform ^
  android ^
  --abi ^
  arm64-v8a ^
  --os-version ^
  24 ^
  --stl ^
  c++_shared ^
  --ndk-version ^
  25 ^
  --output ^
  "C:\\Users\\Sawyer\\AppData\\Local\\Temp\\agp-prefab-staging16999448797774168626\\staged-cli-output" ^
  "C:\\Users\\Sawyer\\.gradle\\caches\\transforms-3\\63a0432d4893379bc8692dca1284b91e\\transformed\\jetified-oboe-1.4.3\\prefab"
