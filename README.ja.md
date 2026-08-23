# M5DUALKEY-DeskCon-RGB-ToF

English: [README.md](README.md)

<p align="center">
  <img src="assets/images/dualkey.jpg" width="700" alt="M5DUALKEY-DeskCon-RGB-ToF">
</p>

M5DUALKEY-DeskCon-RGB-ToFは、M5Stack Chain DualKey、Encoder、Angle、RGB、ToFで構成したmacOS向けのコンパクトなUSB HIDコントローラーです。

標準のキーボード、Consumer Control、マウスホイールイベントを送信します。オーディオ出力の切り替えはRaycast（または他のmacOS自動化ツール）へ委譲し、OS固有処理をファームウェアから分離しています。

## 主な特徴

- オーディオ出力の選択・トグル用ショートカット
- ハードウェアによる音量・ミュート操作
- 起動時キャリブレーションとヒステリシスを備えたオートスクロール
- オーディオ出力とミュートを示す控えめなRGB状態表示
- ToF距離に反応する8 x 8 RGB Matrix表示
- 4台のChainモジュールの自動探索と構成検証

## ハードウェア構成

本プロジェクトでは、USB-Cポートが背面に来る向きを正面として扱います。左右の表記はすべてこの向きから見たものです。正面から見て左から、DualKey、Encoder、Angle、Chain RGB、Chain ToFが固定の物理配置です。

```text
正面図（USB-Cは背面側）

+-----------+-----------+-----------+-----------+-----------+
| DualKey   | Encoder   | Angle     | RGB       | ToF       |
+-----------+-----------+-----------+-----------+-----------+
```

- M5Stack Chain DualKey
- M5Stack Chain Encoder
- M5Stack Chain Angle
- M5Stack Chain RGB
- M5Stack Chain ToF
- macOSへのUSB接続

## 簡易操作一覧

| モジュール | 操作 | 動作 |
| --- | --- | --- |
| DualKey | 左 | ORA4を選択（`Ctrl + Cmd + 1`）、LEDは赤 |
| DualKey | 右 | Studio Displayを選択（`Ctrl + Cmd + 2`）、LEDは黄 |
| DualKey | 左右同時 | 出力をトグル（`Ctrl + Option + S`） |
| Encoder | 回転／押し込み | 音量アップ、音量ダウン、ミュート。Mute中は紫 |
| Angle | 左／中央／右 | 上スクロール、停止、下スクロール。操作中は青 |
| Chain ToF | 手との距離 | 33 msのSINGLE測距。完了値を読むたびに次の測距を明示的にSTART |
| Chain RGB | Matrix表示 | 400 mm以上で消灯し、50 mmへ近づくほど中央のSquareが拡大・高輝度化 |

Raycastを使用するのはDualKeyの3つのオーディオ出力操作だけです。Encoderの音量／ミュートとAngleのスクロールは、USB HIDイベントとしてmacOSへ直接送信されます。

LEDはmacOSから取得した実状態ではなく、ファームウェア内の状態を表示します。起動時の挙動と制約は[操作とLED表示](docs/controls.ja.md)を参照してください。

## 詳細ドキュメント

- [アーキテクチャ](docs/architecture.ja.md)
- [操作とLED表示](docs/controls.ja.md)
- [Raycast連携](docs/raycast.ja.md)
- [開発環境とビルド設定](docs/development.ja.md)

## Project Status

USB HID Keyboard、Consumer Control、Angleオートスクロール、起動時キャリブレーション、RGB状態表示、ToF SINGLE測距、距離連動RGB Matrixを実装済みです。BLE HIDは今後実装予定です。

## ライセンス

MIT License。詳細は[LICENSE](LICENSE)を参照してください。

## Maintainer

omiya-bonsai
