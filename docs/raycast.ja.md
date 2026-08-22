# Raycast連携

English: [raycast.md](raycast.md)

Raycastを使用するのは、DualKeyによる3つのオーディオ出力操作だけです。ファームウェアはORA4やStudio Displayを直接操作せず、キーボードショートカットを送信し、RaycastがmacOS上で実行するコマンドへ割り当てます。

## ショートカット割り当て

| macOSが受信するショートカット | Raycastコマンド |
| --- | --- |
| `Ctrl + Cmd + 1` | ORA4を選択 |
| `Ctrl + Cmd + 2` | Studio Displayを選択 |
| `Ctrl + Option + S` | ORA4／Studio Displayをトグル |

使用するMacに合わせたShell Script、AppleScript、または他のローカル自動化を実行するRaycastコマンドを作成し、上記ショートカットを割り当てます。スクリプト内容、デバイス名、オーディオ切り替えツールはユーザー環境固有のため、このリポジトリでは断定しません。

EncoderのVolume Up、Volume Down、MuteはRaycastを経由せず、標準のUSB HID Consumer ControlとしてmacOSへ直接送信されます。AngleのスクロールもRaycastを経由せず、USB HIDマウスホイールとしてmacOSへ直接送信されます。

同じショートカット構成はBetterTouchTool、Keyboard Maestro、Hammerspoon、Karabiner-Elementsなどでも実現できます。割り当てを変更する場合は、ファームウェアのショートカットと自動化ツール側の設定を一致させてください。
