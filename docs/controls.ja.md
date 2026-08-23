# 操作とLED表示

English: [controls.md](controls.md)

本プロジェクトでは、USB-Cポートが背面に来る向きを正面として扱います。正面から見た物理配置は、左からDualKey、Encoder、Angle、Chain RGB、Chain ToFです。以下の左右操作とLED位置は、すべてこの向きから見た表記です。

## DualKey

| 操作 | HIDショートカット | 想定するRaycast動作 |
| --- | --- | --- |
| 左キー | `Ctrl + Cmd + 1` | ORA4を選択 |
| 右キー | `Ctrl + Cmd + 2` | Studio Displayを選択 |
| 左右同時押し | `Ctrl + Option + E` | 設定したカスタム操作を実行 |

同時押しを最優先します。短い同時押し判定時間を設け、ほぼ同時の押下で先に単独キー処理が実行されることを防ぎます。

これらのキーボードショートカットはRaycastまたは同等のmacOS自動化を使用します。左右同時押しは独立したカスタム操作です。

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

## ToF proximity入力

Chain ToFは測定時間33 msのSINGLEモードで動作します。ファームウェアは`STOP`状態と完了フラグの両方を確認し、距離を読み取ってから次のSINGLE測距を明示的にSTARTします。CONTINUOUSモードは使用しません。

有効距離を前回値75%、新規値25%で平滑化し、500 mm（遠）から100 mm（近）を0〜1のproximity値へ変換します。proximityは量子化後にさらに平滑化します。距離が無効、または500 ms以上更新されない場合はambient値が穏やかに0へ戻ります。

ToFはMatrix ambient用の連続パラメーターとしてだけ使用します。イベント化せず、USB HID Keyboard、Consumer Control、Mouseのいずれも呼び出しません。

## RGB Matrix

Matrixは距離を段階表示しません。ambient時はToF proximityに応じて明るさ、呼吸速度、光の広がり、微細な揺らぎが穏やかに増えます。500 mm未満の有効範囲へ入ると、Matrixのmaster brightnessも50%から85%へ最大15秒間だけ上がります。範囲外または距離無効になると50%へ戻って再armします。15秒経過後は手を範囲内へ置いたままでも延長・再開せず、いったん範囲外へ出してから再接近する必要があります。最新の操作Actionがambient frameを一時的に置き換えます。

| Action | Matrix feedback |
| --- | --- |
| ORA4選択 | 赤、中央から外側へのripple |
| Studio Display選択 | 黄、中央から外側へのripple |
| DualKey左右同時 | 短い白flashからcontraction |
| Volume Up | 紫のexpansion |
| Volume Down | 紫のcontraction |
| Mute | 紫のpulse |
| Scroll Up | 青、下から上への移動 |
| Scroll Down | 青、上から下への移動 |

animationは`millis()`ベースで、main loopをdelayしません。操作feedbackは常にproximity ambientより優先され、DualKey、Encoder、AngleのすべてのActionを通常のmaster brightness 50%で描画します。Action中もproximity値は内部で追従し、15秒枠が残っていればAction終了後のToF ambientで85%へ復帰します。全frame転送は最短80 ms、同一frameは送信せず、Chain APIとoperation statusがともに成功した場合だけ輝度・frame cacheを更新します。frameごとのRGB level制限は従来どおりです。

## LED状態表示

| 内部状態 | LED表示 |
| --- | --- |
| オーディオ出力不明 | DualKey左右とも低輝度standby |
| ORA4選択 | DualKey左LEDが赤（`255, 40, 40`）で呼吸し、右はstandby |
| Studio Display選択 | DualKey右LEDが黄（`255, 220, 0`）で呼吸し、左はstandby |
| ローカルのミュート状態がON | Encoder LEDがBright Purple（`170, 40, 255`） |
| ローカルのミュート状態がOFF | Encoder LEDは低輝度の紫standby |
| Angleが中央かつ停止 | Angle LEDは低輝度の青standby |
| Angle操作中 | Angle LEDがBright Blue（`40, 140, 255`）で、操作量に応じて輝度が変化 |

選択中の出力LEDとミュートLEDは、色相を固定した1/f風の呼吸アニメーションで表示します。Angle LEDは呼吸せず、正規化した操作量へ即応し、中央へ戻って停止するとstandbyへ戻ります。

起動時は、DualKey左を赤、DualKey右を黄、Encoderを紫、Angleを青、Chain RGBの順に点灯します。その後に通常の状態表示とambient描画を開始します。Chain ToF本体のLEDはこの起動演出へ参加しません。

起動時のオーディオ出力は明示的に`UNKNOWN`とし、特定の出力先を仮定せずDualKey左右LEDをstandbyにします。左または右の直接選択後に内部状態が確定します。左右同時押しのカスタム操作とToF proximityは出力状態を変更しません。

ローカルのミュート状態はOFFで起動し、EncoderからMuteコマンドを送るたびに反転するため、最初の押下で紫のstandbyから呼吸表示へ変化します。出力先もミュートもmacOSから読み戻していません。macOSや別デバイスから操作するとLEDと実状態がずれる可能性があります。出力表示は左または右キーの直接選択で再同期できます。ミュートLEDはローカル操作状態の目安として扱ってください。
