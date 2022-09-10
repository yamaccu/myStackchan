自分用のスタックチャンのソースコード 

## 環境

PlatformIO  
[env:m5stack-grey]  
platform = espressif32  
board = m5stack-grey  
framework = arduino  

## 使用ライブラリ

[M5Unified](https://github.com/m5stack/M5Unified)  
[m5stack-avatar](https://github.com/meganetaaan/m5stack-avatar)  
[aquestalk](https://www.a-quest.com/) 　⇒AquesTalk ESP32  
[ServoEasing](https://github.com/ArminJo/ServoEasing)  
[ArduinoJson](https://github.com/bblanchon/ArduinoJson)

## 改造箇所
音声出力をDAC25⇒DAC26に変更、D級アンプとスピーカーを追加。  
ソース変更箇所：M5Unified.cpp 354/355/357行目  
[D級アンプ](https://akizukidenshi.com/catalog/g/gK-08217/)  
[スピーカー](https://akizukidenshi.com/catalog/g/gP-12494/)  

## トーク内容を外部から編集

以下の方法で、ソースを変更せずにトーク内容を編集しています。  

1. SSSAPIというサービスを使ってWEB APIを作成
2. M5StackからAPIを読み取って、AquesTalkで音声出力


## 参考HP
■作成ブログ  
[スタックチャンを作ってみた](https://yamaccu.github.io/tech/20220910_stackchan)  

■サーボ制御  
[stack-chan-tester](https://github.com/mongonta0716/stack-chan-tester)   
[M5Stack(ESP32)でサーボモータを使う](https://yamaccu.github.io/tils/20220801-M5Stack-servo)  

■音声出力  
[M5StackでAquesTalkを使って音声出力する](https://yamaccu.github.io/tils/20220829-M5Stack-AquesTalk)  
[SSSAPI](https://sssapi.app/)

■天気予報  
[天気予報API](https://www.drk7.jp/weather/)  
[M5Stackでwifiに接続し天気予報JSONを取得する](https://yamaccu.github.io/tils/20220808-M5Stack-JSON)  
[M5StackでLCD画面に画像を表示する](https://yamaccu.github.io/tils/20220908-M5Stack-image)  
