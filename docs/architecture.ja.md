# アーキテクチャ

English: [architecture.md](architecture.md)

## システム全体構成

本プロジェクトでは、USB-Cポートが背面に来る向きを正面として扱います。正面から見た固定の物理配置は、左からESP32-S3搭載のChain DualKey、Encoder、Angle、Chain RGB、Chain ToFです。4台の周辺モジュールをDualKey右側ポート（`RX=GPIO5`、`TX=GPIO6`）のM5Chain UARTデイジーチェーンで接続しています。

```text
正面図（USB-Cは背面側）

+-----------+-----------+-----------+-----------+-----------+
| DualKey   | Encoder   | Angle     | RGB       | ToF       |
+-----------+-----------+-----------+-----------+-----------+

DualKey (ESP32-S3) -> Encoder -> Angle -> RGB -> ToF
                  M5Chain UARTデイジーチェーン
```

ファームウェアはChain IDを固定せず、デバイスタイプから必要な周辺モジュールを探索します。デバイス数とリストを正常に取得し、Encoder、Angle、RGB、ToFをすべて発見できた場合にChain列挙成功とします。追加デバイスが列挙されても取得済みIDは無効化しません。その後のデバイス固有設定が失敗した場合も、ログを残して無関係なデバイスIDを維持します。DualKeyのボタンはESP32-S3で直接読み取ります。Encoderの回転とボタンはUSB HID Consumer Control、AngleはUSB HIDマウスホイールとして送信します。

3つのHID経路は、それぞれ次の責務を持ちます。ToFは3経路すべての外側にあり、HID入力を送信しません。

```text
DualKey直接選択 ------------+
DualKey左右同時カスタム操作 --+--> USB HID Keyboard --> Raycast / macOS自動化
                                        +--> Audio出力または設定したカスタム操作

Encoder回転／押下 -------------> USB HID Consumer Control --> Volume / Mute
Angle位置 ---------------------> USB HID Mouse -------------> Scroll
ToF距離 -----------------------> HID出力なし
```

DualKeyのキーボードショートカットはRaycastを使用します。ESP32側はORA4やStudio Displayを直接操作せず、実際の切り替えはRaycastとmacOS側に登録したスクリプトが行います。Encoderの音量／ミュートとAngleのマウスホイールは、Raycastを経由せずUSB HIDからmacOSへ直接送信されます。ToF距離は表示入力専用です。

センサーから表示までの経路は、これらのHID経路から独立しています。

```text
ToF SINGLE START
      |
      v
STOP + COMPLETE -> 有効距離 -> 75/25平滑化
                                    |
                                    v
                         100〜500 mm mapping
                         量子化 + 平滑化
                                    |
                         proximity ambient parameter
                                    |
                                    v
                         明るさ／呼吸／広がり／
                              微細な揺らぎ

DualKey／Encoder／Angle Action -> MatrixAnimation state --+
                                                           +-> Action優先renderer
ToF proximity ambient -------------------------------------+          |
                                                                      v
                                                            80 ms frame制限
                                                                      |
                                                           差分frame + API成功
                                                                      |
                                                                 frame cache
```

ToFは実機確認済みの33 ms SINGLEシーケンスを維持し、完了した距離を読むたびに次の測距を明示的にSTARTします。有効距離は75/25で平滑化し、500 msで失効して、ambient描画用の範囲内proximity値へ変換します。Keyboard、Consumer Control、Mouseは呼び出しません。ToF本体のLEDは起動演出へ参加しません。

Matrixは絶対距離を段階表示しません。無操作時は中央付近の淡い光を表示し、明るさ、呼吸速度、広がり、微細な揺らぎをproximityに応じて変化させます。DualKey、Encoder、Angleの入力Actionが発生すると、最新操作がambient frameを一時的に置き換え、ripple、contraction、expansion、pulse、方向移動を表示します。描画は`millis()`ベースで、入力、HID、ToF処理をブロックしません。

## 責務分離

- **入力処理:** DualKeyとEncoderのデバウンス、DualKey同時押し判定、Angle位置の解釈
- **HID出力:** DualKeyのキーボードショートカットはRaycastへ、Consumer ControlとマウスホイールはmacOSへ直接送信し、ToFにはHID経路を設けない
- **Angle制御:** 起動時の中央キャリブレーション、ヒステリシス、スクロール間隔の計算
- **Chain検証:** Encoder、Angle、RGB、ToFの型が存在することを検証し、以後の設定失敗を該当デバイスへ分離
- **proximity入力:** 有効な平滑化済みToF SINGLE距離を量子化・平滑化したambient parameterへ変換
- **Matrix feedback:** 小さなnon-blocking state machineで最新操作Actionをproximity ambientより優先して描画
- **LED状態:** DualKeyから最後に選択した出力とローカルのミュート状態を保持し、専用のLED更新関数で表示
- **macOS自動化:** DualKeyのキーボードショートカットをファームウェア外のスクリプトへ割り当て

## LED状態の境界

LEDの固定対応は、DualKey左 = ORA4のBright Red、DualKey右 = Studio DisplayのBright Yellow、Encoder = MuteのBright Purple、Angle = スクロール操作量を示すBright Blueです。選択中の出力表示とミュート表示は1/f風に呼吸し、Angle LEDは正規化した操作量に応じて輝度が変化して停止後にstandbyへ戻ります。

LED状態はM5Stack側で管理します。macOSから状態を取得する経路はないため、OSの確定的な実状態ではありません。macOS、別のキーボード、別アプリから出力先やミュートを変更すると、LED表示と実状態がずれることがあります。Matrixのmaster brightnessはコンパイル時に50%を上限とし、base ambient frameは5〜12%のRGB level、proximity加算は最大8%と微細な揺らぎ、feedbackは短時間に制限します。状態保持とLED更新を入力処理から分離しているため、将来BLE状態やホストからのフィードバックを追加できます。
