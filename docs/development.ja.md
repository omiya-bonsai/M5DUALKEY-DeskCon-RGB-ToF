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

## LEDとMatrix実装

本プロジェクトでは、USB-Cポートが背面に来る向きを正面として扱います。この正面から見た物理チェーンは、左からDualKey、Encoder、Angle、Chain RGB、Chain ToFです。固定された右側接続では`RX=GPIO5`、`TX=GPIO6`、115200 baudを使用し、UART自動探索は行いません。DualKey本体の2個のWS2812BはAdafruit NeoPixelで制御し、信号はGPIO 21、電源イネーブルはGPIO 40です。物理的な左キーはpixel 0、右キーはpixel 1に対応します。EncoderとAngleのLEDは、デバイス探索で得たIDに対してM5Chain 1.0.8の`setRGBLight()`と`setRGBValue()`を使います。

8 x 8 Matrixには独立した安全境界があります。`RGB_MATRIX_MAX_BRIGHTNESS`は85、`MATRIX_NORMAL_BRIGHTNESS`は50、`MATRIX_TOF_BOOST_BRIGHTNESS`は85とし、両方を`static_assert`で絶対上限以下に固定します。通常idleとすべての操作Actionはmaster brightness 50%を使用します。arm済みのToF proximity ambient期間だけ85%を選択でき、物体を置いたままでも15秒で終了して延長されません。各frameのRGB値には従来の追加level制限を維持します。

## Chain初期化と処理周期

Chain列挙では、デバイス数とリストを正常に取得し、Encoder、Angle、RGB、ToFのIDをすべて発見することを必須とします。総デバイス数が4と一致することは要求しません。Encoder、RGB、ToFの初期化でもChain戻り値とoperation statusの両方を検証しますが、デバイス固有の設定失敗で他の列挙済みIDを消去しません。

Encoder回転は15 ms、ボタンは25 ms周期で読み取ります。Angleは既存の20 ms周期を維持します。ToF完了確認は40 ms周期、64 pixelのMatrix全体更新は最短80 ms周期（12.5 fps）に制限します。同一Matrix frameは再送せず、`CHAIN_OK`とoperation statusの両方が成功した場合だけcacheを進めます。失敗frameはcacheされないため、後続frameで再試行されます。これらの制限により、115200 baudのChain UARTを4台で共有します。

ToFはv0.1.0の通信シーケンスを維持します。`SINGLE`、測定時間33 ms、明示的な`START`、`STOP + COMPLETE`確認、距離読取、完了ごとの明示的な再STARTの順序です。有効距離は既存の75/25平滑化を行い、500 msで失効します。Matrix ambient処理はこの結果を利用し、M5Chain初期化、列挙、typeからIDへの探索、測定API順序を変更しません。ToF処理からUSB HIDを呼び出す経路はありません。

## ToF proximity tuning

80 msごとのMatrix更新時に、有効な平滑化済み距離を遠点・近点から0〜1へ線形変換します。targetを量子化してからさらにlow-pass処理することで、小さなframe変化を抑え、bus trafficを増やしません。無効値または古い距離ではtargetを0とします。この処理は表示状態だけを変化させ、イベントは生成しません。

主な調整定数:

| 定数 | 初期値 | 用途 |
| --- | ---: | --- |
| `TOF_AMBIENT_NEAR_MM` | 100 mm | proximity 1へ割り当てる距離 |
| `TOF_AMBIENT_FAR_MM` | 500 mm | proximity 0へ割り当てる距離 |
| `TOF_AMBIENT_PROXIMITY_STEPS` | 16 | 表示平滑化前の量子化step数 |
| `TOF_AMBIENT_PROXIMITY_SMOOTHING` | 0.16 | frameごとにtargetへ近づける割合 |
| `TOF_AMBIENT_SPEED_BOOST` | 0.60 | 呼吸速度の最大増加率 |
| `TOF_AMBIENT_BRIGHTNESS_BOOST` | 0.08 | ambient RGB levelの最大加算値 |
| `TOF_AMBIENT_SPREAD_BOOST` | 0.35 | radial falloffの最大減少量 |
| `TOF_AMBIENT_SHIMMER_MAX` | 0.12 | 微細なripple modulationの最大値 |
| `TOF_BRIGHTNESS_BOOST_DURATION_MS` | 15000 ms | Matrix master brightness 85%の最大継続時間 |

## Matrix animation scheduling

`MatrixAnimation`が保持するのは現在のanimation、開始時刻、継続時間だけです。DualKey、Encoder、Angleの新しいActionは古いanimationを置き換えます。`updateMatrix()`は`millis()`からframeを計算し、`delay()`を使用しません。赤／黄ripple、白flash-contraction、紫expansion／contraction／pulse、青の垂直移動を実装しています。Action描画はToF proximity ambientより優先され、master brightnessを50%へ戻します。Action表示中もproximityは内部で追従し、終了時にToF boost枠が残っていればambientを85%へ戻します。輝度変更はAPI戻り値とoperation statusの両方が成功した場合だけcacheし、85%からの低下に失敗した場合は次のAction frameを抑止して85%で操作feedbackを表示しません。

## Angleのキャリブレーションとスクロール

電源投入またはリセット前にAngleを物理的な中央へ置いてください。起動時キャリブレーションは10 ms間隔で有効40サンプルを平均し、最大80回まで取得を試行します。その平均値を次回再起動まで使用します。

`M5DUALKEY-DeskCon-RGB-ToF.ino`の主な調整定数:

| 定数 | 初期値 | 用途 |
| --- | ---: | --- |
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
