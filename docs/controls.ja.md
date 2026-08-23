# 操作とLED表示

English: [controls.md](controls.md)

本プロジェクトでは、USB-Cポートが背面に来る向きを正面として扱います。正面から見た物理配置は、左からDualKey、Encoder、Angle、Chain RGB、Chain ToFです。以下の左右操作とLED位置は、すべてこの向きから見た表記です。

## DualKey

| 操作 | HIDショートカット | 想定するRaycast動作 |
| --- | --- | --- |
| 左キー | `Ctrl + Cmd + 1` | ORA4を選択 |
| 右キー | `Ctrl + Cmd + 2` | Studio Displayを選択 |
| 左右同時押し | `Ctrl + Option + S` | ORA4／Studio Displayをトグル |

同時押しを最優先します。短い同時押し判定時間を設け、ほぼ同時の押下で先に単独キー処理が実行されることを防ぎます。

Raycastを使用する操作は、この3つだけです。

## Encoder

| 操作 | USB HID Consumer Control |
| --- | --- |
| 時計回り | Volume Up |
| 反時計回り | Volume Down |
| 押し込み | Mute / Unmute |

Encoder操作はUSB HID Consumer ControlとしてmacOSへ直接送信され、Raycastを経由しません。

## Angle

| 位置 | 動作 |
| --- | --- |
| 左 | 上方向へオートスクロール |
| 中央 | スクロール停止 |
| 右 | 下方向へオートスクロール |

AngleのスクロールはUSB HIDマウスホイールとしてmacOSへ直接送信され、Raycastを経由しません。

起動時にAngleの中央位置をキャリブレーションします。開始と停止で異なるしきい値を使うヒステリシスにより、中央付近の小さな揺れでスクロールが頻繁に再開することを防ぎます。開始しきい値の外側では、中央からの距離に応じてx^1.5の速度カーブで加速します。

起動中はAngleを物理的な中央位置に置いてください。

## ToFとRGB Matrix

Chain ToFは測定時間33 msのSINGLEモードで動作します。ファームウェアは`STOP`状態と完了フラグの両方を確認し、距離を読み取ってから次のSINGLE測距を明示的にSTARTします。START失敗時は制限された間隔で再試行し、CONTINUOUSモードは使用しません。

有効かつ新しい距離だけがChain RGBの点灯表示を駆動します。400 mm以上ではMatrixを消灯します。50 mmへ近づくほど中央のSquareが2 x 2から8 x 8へ拡大し、明るくなります。距離は前回値75%、新規値25%で平滑化します。500 msの間に正常な距離を取得できない場合は無効化し、Matrixを消灯します。

## LED状態表示

| 内部状態 | LED表示 |
| --- | --- |
| オーディオ出力不明 | DualKey左右とも消灯 |
| ORA4選択 | DualKey左LEDがBright Red（`255, 40, 40`） |
| Studio Display選択 | DualKey右LEDがBright Yellow（`255, 220, 0`） |
| ローカルのミュート状態がON | Encoder LEDがBright Purple（`170, 40, 255`） |
| ローカルのミュート状態がOFF | Encoder LEDが消灯 |
| Angleが中央かつ停止 | Angle LEDが消灯 |
| Angle操作中 | Angle LEDがBright Blue（`40, 140, 255`）で、操作量に応じて輝度が変化 |

選択中の出力LEDとミュートLEDは、色相を固定した1/f風の呼吸アニメーションで表示します。Angle LEDは呼吸せず、正規化した操作量へ即応し、中央へ戻って停止すると約150 msでフェードアウトします。

起動時は、DualKey左を赤、DualKey右を黄、Encoderを紫、Angleを青、Chain RGBの順に点灯します。その後、各色を維持したまま約250 msのREADY表示を行い、同時にフェードアウトします。Chain ToF本体のLEDはこの起動演出へ参加しません。

起動時のオーディオ出力は明示的に`UNKNOWN`とし、特定の出力先を仮定せずDualKey左右LEDを消灯します。左または右の直接選択後に内部状態が確定します。左右同時押しは、内部状態が既知の場合だけ状態を反転します。起動後の最初の操作がトグルだった場合、結果を推測できないため不明状態と消灯を維持し、直接選択後から表示します。

ローカルのミュート状態はOFFで起動し、EncoderからMuteコマンドを送るたびに反転するため、最初の押下で紫に点灯します。出力先もミュートもmacOSから読み戻していません。macOSや別デバイスから操作するとLEDと実状態がずれる可能性があります。出力表示は左または右キーの直接選択で再同期できます。ミュートLEDはローカル操作状態の目安として扱ってください。
