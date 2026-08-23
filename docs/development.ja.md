# 開発

English: [development.md](development.md)

## 開発環境

- Arduino IDEまたはArduino CLI
- Board: `M5ChainDualKey`（ESP32-S3）
- M5Stack ESP32ボードパッケージ
- M5Chain 1.0.8
- M5Unified
- Adafruit NeoPixel 1.15.2以降
- ESP32 USB HIDライブラリ（`USBHIDKeyboard`、`USBHIDConsumerControl`、`USBHIDMouse`）

## 重要なボード設定

現在のファームウェアはBLE HIDではなく、ネイティブUSB HIDを使用します。USB Modeは**USB-OTG (TinyUSB)**を選択してください。HIDと同時にSerialコンソールを使う場合は**USB CDC On Boot**を有効にします。同じネイティブUSB接続から書き込む場合はUpload Modeを**USB-OTG CDC (TinyUSB)**にできます。HIDファームウェアの書き込み後にポートが再表示されない場合は、ボードのダウンロード／ブート操作で書き込みモードへ入れてください。

ボードメニューはM5Stackパッケージのバージョンにより異なります。USB HIDヘッダーのコンパイルエラーをファームウェアの問題と判断する前に、TinyUSBが選択されていることを確認してください。BLE HIDは今後実装予定で、現時点では未実装です。

## LED実装

本プロジェクトでは、USB-Cポートが背面に来る向きを正面として扱います。この正面から見た物理チェーンは、左からDualKey、Encoder、Angle、Chain RGB、Chain ToFです。固定された右側接続では`RX=GPIO5`、`TX=GPIO6`を使用し、UART自動探索は行いません。DualKey本体の2個のWS2812BはAdafruit NeoPixelで制御し、信号はGPIO 21、電源イネーブルはGPIO 40です。物理的な左キーはpixel 0、右キーはpixel 1に対応します。ChainモジュールLEDは、デバイス探索で得たIDに対してM5Chain 1.0.8の`setRGBLight()`と`setRGBValue()`を使います。Chain LEDのマスター輝度はAPI上限の100です。LED値はChain APIとデバイスのoperation statusがともに成功した場合だけキャッシュします。

## Chain初期化と処理周期

Chain列挙では、デバイス数とリストを正常に取得し、Encoder、Angle、RGB、ToFのIDをすべて発見することを必須とします。総デバイス数が4と一致することは要求しません。Encoder、RGB、ToFの初期化でもChain戻り値とoperation statusの両方を検証しますが、デバイス固有の設定失敗で他の列挙済みIDを消去しません。

Encoder回転は10 ms、ボタンは20 ms周期で読み取ります。Angleは既存の20 ms周期を維持します。ToF完了確認は40 ms周期、64 pixelのMatrix全体更新は最短80 ms周期に制限し、115200 baudのChain UARTを4台で共有できるようにします。

ToFは起動演出とAngleキャリブレーションの後に設定します。`SINGLE`、測定時間33 ms、明示的な`START`、`STOP + COMPLETE`確認、距離読取、明示的な再STARTの順序です。START失敗時は250 ms間隔で再試行します。有効距離は既存の75/25平滑化を行い、500 msで失効します。

## Angleのキャリブレーションとスクロール

電源投入またはリセット前にAngleを物理的な中央へ置いてください。起動時キャリブレーションは10 ms間隔で40サンプルを平均し、その平均値を次回再起動まで使用します。

`M5DUALKEY-DeskCon-RGB-ToF.ino`の主な調整定数:

| 定数 | 初期値 | 用途 |
| --- | ---: | --- |
| `ANGLE_CALIBRATION_SAMPLES` | 40 | 起動時サンプル数 |
| `ANGLE_CALIBRATION_INTERVAL_MS` | 10 ms | サンプル間隔 |
| `ANGLE_CALIBRATION_MAX_ATTEMPTS` | 80 | 有効40サンプルを取得する最大試行数 |
| `ANGLE_STOP_OFFSET` | 95 | 中央へ戻った際の停止しきい値 |
| `ANGLE_START_OFFSET` | 135 | 開始しきい値 |
| `ANGLE_READ_INTERVAL_MS` | 20 ms | ADC読み取り周期 |
| `SCROLL_SLOWEST_INTERVAL_MS` | 220 ms | 最低速のホイール送信間隔 |
| `SCROLL_FASTEST_INTERVAL_MS` | 25 ms | 最高速のホイール送信間隔 |
| `SCROLL_BASE_SPEED` | 0.35 | 開始しきい値直後の初期速度 |

加速カーブは`normalizedDistance^1.5`です。開始と停止のオフセットを分けてヒステリシスを持たせています。

## ビルド例

```sh
arduino-cli compile \
  --fqbn 'm5stack:esp32:m5stack_chain_dualkey:USBMode=default,CDCOnBoot=cdc,UploadMode=cdc,FlashSize=8M,PartitionScheme=default_8MB' \
  M5DUALKEY-DeskCon-RGB-ToF
```

書き込み前に、選択中のボードパッケージとポートを確認してください。コンパイル成功だけでは、実機のDualKey、Encoder、Angle、RGB、ToFチェーンの確認を代替できません。
