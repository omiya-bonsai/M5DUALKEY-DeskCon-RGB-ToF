# アーキテクチャ

English: [architecture.md](architecture.md)

## システム全体構成

本プロジェクトでは、USB-Cポートが背面に来る向きを正面として扱います。正面から見た正式な物理配置は、左からESP32-S3搭載のChain DualKey、Encoder、Angleです。3モジュールをM5Chain UARTデイジーチェーンで接続しています。

```text
正面図（USB-Cは背面側）

+-----------+-----------+-----------+
| DualKey   | Encoder   | Angle     |
+-----------+-----------+-----------+

DualKey (ESP32-S3) -> Encoder -> Angle
          M5Chain UARTデイジーチェーン
```

ファームウェアはChain IDを固定せず、デバイスタイプからEncoderとAngleを探索します。DualKeyのボタンはESP32-S3で直接読み取ります。Encoderの回転とボタンはUSB HID Consumer Control、AngleはUSB HIDマウスホイールとして送信します。

3つのHID経路は、それぞれ次の責務を持ちます。

```text
M5DUALKEY-DeskCon-RGB-ToF
        |
        +--> USB HID Keyboard
        |       |
        |       v
        |    Raycast
        |       |
        |       v
        |   Audio Output Switching
        |
        +--> USB HID Consumer Control
        |       |
        |       v
        |   Volume / Mute
        |
        +--> USB HID Mouse
                |
                v
             Scroll
```

Raycastを使用するのはDualKeyのキーボードショートカットだけです。ESP32側はORA4やStudio Displayを直接操作せず、実際の切り替えはRaycastとmacOS側に登録したスクリプトが行います。Encoderの音量／ミュートとAngleのマウスホイールは、Raycastを経由せずUSB HIDからmacOSへ直接送信されます。

## 責務分離

- **入力処理:** DualKeyとEncoderのデバウンス、DualKey同時押し判定、Angle位置の解釈
- **HID出力:** DualKeyのキーボードショートカットはRaycastへ、Consumer ControlとマウスホイールはmacOSへ直接送信
- **Angle制御:** 起動時の中央キャリブレーション、ヒステリシス、スクロール間隔の計算
- **LED状態:** DualKeyから最後に選択した出力とローカルのミュート状態を保持し、専用のLED更新関数で表示
- **macOS自動化:** DualKeyの3つのオーディオ出力ショートカットだけをファームウェア外のスクリプトへ割り当て

## LED状態の境界

LEDの固定対応は、DualKey左 = ORA4のBright Red、DualKey右 = Studio DisplayのBright Yellow、Encoder = MuteのBright Purple、Angle = スクロール操作量を示すBright Blueです。選択中の出力表示とミュート表示は1/f風に呼吸し、Angle LEDは正規化した操作量に応じて輝度が変化して停止後にフェードアウトします。

LED状態はM5Stack側で管理します。macOSから状態を取得する経路はないため、OSの確定的な実状態ではありません。macOS、別のキーボード、別アプリから出力先やミュートを変更すると、LED表示と実状態がずれることがあります。状態保持とLED更新を入力処理から分離しているため、将来BLE状態やホストからのフィードバックを追加できます。
