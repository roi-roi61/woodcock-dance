# Woodcock Dance v0.3.4

Windows 10/11 x64向けの、無音・常に最前面のアメリカヤマシギ常駐デモです。

- [仕様書](SPECIFICATION.md)
- [利用条件・二次利用方針](TERMS.md)
- [セキュリティ報告](SECURITY.md)
- [テストレポート](TEST-REPORT.md)

## 現在の実装

- 添付した1枚目のアメリカヤマシギを基準個体として、左右反転せず使用
- 初回は基準個体の見えている左端をメインモニターの作業領域左端に合わせて開始
- 基準個体が先頭IDとして常に固定され、追加個体は論理配列の末尾へ追加
- 周回完了時に追加個体を1匹生成し、画面左外から通常間隔で遅れて参加
- 追加個体も同じ左→右の移動に同期し、最初の個体を追い越さない
- 画面全体の横断は約3分。隊列全体が右端から完全に退出すると次の周回へ移る
- 18匹に到達した後は増えず、同じ1列を維持
- 移動中も常時ダンスし、上下約8%、左右約4%、傾き約6度、軽い伸縮で大きく動く
- ダンス周期は約3.5秒で、歩行を止めない
- 添付した7種類の画像を使用し、透明ピクセルを保ったまま最近傍のピクセル描画を使用
- 初回起動時のサイズ選択画面は表示せず、初期サイズは144px（中サイズ）
- タスクトレイから96px・144px・192pxへサイズ変更でき、選択は次回起動時にも保存される
- クリック透過はせず、個体が1匹だけのときに基準個体をクリックすると約10秒間3匹に増える
- 2匹以上いるときのクリックは無音・無反応で、匹数と順番を変えない
- 3匹化中も同じタイミングで踊りながら移動し、再クリックは無視
- 多重起動を防止し、常に1つの常駐インスタンスだけを動かす
- 7枚の画像を実行ファイルにも埋め込み、`WoodcockDance.exe`単体でも起動できる
- `Ctrl+Alt+W`で表示／非表示を切り替え
- タスクトレイから表示、サイズ変更、自動起動、終了を操作
- 初回起動時にWindowsログオン時の自動起動を確認し、選択結果を保存
- 自動起動はタスクトレイから後で変更できる

## ビルド

Visual Studio 2022 Build ToolsとCMakeが必要です。

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

生成物は`build\\Release\\WoodcockDance.exe`です。実行ファイルと同じ場所に`assets`フォルダーがあれば外部画像を読み込みます。外部画像は容量・寸法を検査し、読み込めない場合は実行ファイルに埋め込んだ7枚の画像へ自動的に切り替わります。そのため、配布フォルダーから`WoodcockDance.exe`だけを取り出しても起動できます。

配布版には`CMakeLists.txt`と`src\\main.cpp`も同梱しており、ソースコードを確認・改変して再ビルドできます。

## ダウンロード

GitHub ReleasesのWindows x64 ZIPを展開して`WoodcockDance.exe`を実行してください。インストーラーや管理者権限は不要です。

Releaseに掲載するSHA-256と、ダウンロードしたZIPのハッシュを照合してください。Windows用バイナリが未署名の場合は、その旨をReleaseに明記します。

## 操作

- 個体が1匹のときにキャラクターをクリック：3匹の同期ダンスを開始
- 個体が2匹以上のときにクリック：何も起こらない
- `Ctrl+Alt+W`：表示／非表示
- タスクトレイを右クリック：表示／非表示、サイズ変更、自動起動、終了
- 初回起動時の自動起動確認で「はい」を選ぶと、Windowsログオン時に自動起動します
- ドラッグ移動はできません

## ライセンス

自作コードはMITです。画像素材は作者が作成したプロジェクト素材として、商用利用・改変・再配布を許可し、クレジット表記は任意としています。詳細と第三者の規約が関係する場合の扱いは`ASSET-NOTICE.txt`と`TERMS.md`を確認してください。

アニメーションは各PNGを最近傍変形して動かす構成です。追加個体は個別の画像を周回ごとに選び、選ばれた見た目をその周回中は固定します。

## English

Woodcock Dance v0.3.4 is a silent, always-on-top American woodcock desktop animation for Windows 10/11 x64.

- Download the Windows x64 ZIP from GitHub Releases and run `WoodcockDance.exe`.
- No installer, administrator permission, login, network access, telemetry, advertisements, or paid service is required.
- On first launch, the app asks whether to start automatically when you sign in to Windows. “Yes” is selected by default, the choice is saved, and it can be changed later from the tray menu.
- The seven included PNG assets are project-owner-created assets. Commercial use, modification, redistribution, and distribution of derivatives are allowed; credit is optional. See `ASSET-NOTICE.txt` and `TERMS.md` for the full conditions and third-party-rights caveat.
- The source code is MIT-licensed. See `SECURITY.md` for private vulnerability reporting guidance and `TEST-REPORT.md` for the release verification record.
