# findicon

[English](README.md)

Nerd Fontsのアイコンを単語で検索し、番号付きPNGで見比べて、選んだ
グリフのコードポイントをJSONで確定する、オフラインのC++20 CLIです。
Codex向けの[findiconスキル](.agents/skills/findicon/SKILL.md)を同梱しています。

## cloneから使い始める

CMake 3.24以上、C++20コンパイラ、ビルドツールが必要です。Windowsでは
Visual StudioのC++ Build ToolsとWindows SDKをインストールしてください。

```sh
git clone https://github.com/poicurr/FindIcon.git
cd FindIcon
cmake --preset release
cmake --build --preset release --parallel
```

初回configure時に、バージョンとSHA-256を固定したFreeType・libpng・zlibの
ソースをダウンロードし、静的にビルドします。実行時のネット接続、Python、
ブラウザ、GPU、ウィンドウシステム、HarfBuzzは不要です。

実行ファイルはVisual Studioの場合`build/release/Release/findicon.exe`、
単一構成ジェネレータの場合`build/release/findicon`に生成されます。
以下の`findicon`をそのパスに置き換えるか、実行ファイルのディレクトリを
`PATH`に追加してください。PowerShellで引用符付きパスを実行する場合は、
`& "C:\path\findicon.exe" search settings --text`のように指定します。

## Codexから使う

clone先をCodexで開き、例えば次のように依頼します。

```text
$findicon を使って20 pxのImGuiツールバー用の設定アイコンを探して。
画像を確認して選び、正確なコードポイントとC++のUTF-8文字列を返して。
```

リポジトリ内の`.agents/skills/findicon`がスキルの本体です。
別プロジェクトから使う場合は、このフォルダ全体をそのプロジェクトの
`.agents/skills/`にコピーします。個人用に共通化する場合の配置先は
`~/.agents/skills/`です。CLIとフォントはスキルとは別なので、clone先を
保持してCodexにパスを伝えるか、ビルド済みCLIを`PATH`に置いてください。
[Codexのスキル検出仕様](https://developers.openai.com/codex/skills/)も参照できます。
グローバル設定の変更やMCPサーバーの登録は不要です。

スキルは次の流れで候補を絞り込みます。

1. 用途を短い関連単語へ展開し、必要に応じてファミリーを指定して検索。
2. 12〜24件程度を、実際のUIサイズでPNGに描画。
3. PNGを画像ツールで開き、意味、線の太さ、小サイズでの判読性を比較。
4. 選んだ番号を、そのPNGと同時生成されたJSONから解決。
5. ID・コードポイント・カタログ版・フォント・用途を引き渡す。

画像を確認できない場合は候補提示にとどめ、目視確認済みとは報告しません。
候補が合わなければ、単語やファミリーを変えて再検索します。

## 検索とプレビュー

```sh
findicon search settings gear cog --family cod,md --limit 12 --text
findicon preview settings gear cog --family cod,md --limit 12 --size 20 --out out/Settings.png --text
findicon info nf-cod-settings_gear
findicon families --text
```

`preview`はPNGと同じ名前のJSONを必ず保存します。`--text`は標準出力だけを
短くし、JSON保存は維持します。JSONには番号、`id`、`codepoint`、
`codepoint_decimal`、`utf8`、別名、フォント情報、セル位置、メトリクス、
`supported`、`clipped`が含まれます。選択するセルは
`supported: true`かつ`clipped: false`を確認してください。

検索の仕様は次のとおりです。

- 複数単語はOR検索。完全一致、単語境界一致、複数単語への一致などで順位付け。
- 名前の英字は大文字小文字を区別せず、`nf-md-cog`と`md-cog`の両方に対応。
- 同一コードポイントは重複排除し、別名は`aliases`に保持。
- `--family cod,md`で絞り込み。フィルタは別名のグループ化より先に適用。
- `--limit`は1〜256、既定値24。`--offset`でページ送り。
- 空検索には`--all`が必要。不明なファミリーや不正な引数はエラー。
- `設定`、`移動`、`回転`、`拡縮`、`拡大`、`縮小`、`保存`、`削除`、
  `検索`、`表示`、`非表示`、`再生`、`停止`を小さな辞書で英語へ展開。
  自然言語検索や埋め込み検索ではありません。

```sh
findicon preview 回転 拡縮 --family md --out out/Transform.png --theme light
findicon preview --id nf-cod-settings_gear --id U+F0493 --out out/Selected.png
findicon preview --all --family cod --limit 24 --offset 24 --out out/Codicons02.png
findicon preview settings --font /path/to/ActualUiFont.ttf --out out/Actual.png
```

`--id`は指定順を保ち、コードポイントを重複排除します。検索語、ファミリー、
ページ指定とは併用できません。`info`はIDのほか、`U+...`、`0x...`、
4〜6桁の16進数、単一UTF-8文字も受け付けます。

| プレビューオプション | 既定値 | 用途 |
| --- | --- | --- |
| `--size` | `20` | 実サイズ、8〜96 px |
| `--large-size` | `64` | 拡大確認用、8〜256 px |
| `--columns` | `4` | 列数、1〜8 |
| `--theme` | `dark` | `dark`または`light` |
| `--out` | `Candidates.png` | PNG保存先。親ディレクトリも作成 |
| `--manifest` | PNGと同じstemの`.json` | 番号とグリフの対応表 |
| `--font` | 同梱Symbols Only | 確認対象フォント |
| `--label-font` | 同梱DejaVu Sans | ラベル用フォント |
| `--strict` | 無効 | 欠落・非対応・クリップがあれば終了値4 |

各グリフは指定ピクセルサイズと実メトリクスで描画します。輪郭をセルいっぱいに
引き伸ばしません。欠落グリフは明示し、`.notdef`を正常な候補として扱いません。
大きすぎる画像は上限で拒否されるため、候補数やサイズを下げてください。

## ImGuiへの引き渡し

コードポイントの意味はフォントに依存します。同じフォントとカタログ版を
使い、選択したID・コードポイント・版を消費側プロジェクトに残してください。
例えばNerd Fonts 3.5.1の`nf-cod-settings_gear`は`U+EB51`です。

```cpp
// Nerd Fonts 3.5.1, nf-cod-settings_gear, U+EB51.
constexpr char SETTINGS_ICON[] = "\xEE\xAD\x91";
ImGui::Button(SETTINGS_ICON);
```

`SymbolsNerdFont-Regular.ttf`をImGuiのフォントアトラスへ読み込むか、
`ImFontConfig::MergeMode`でUIフォントへマージします。古いアトラス方式では、
対象コードポイントを含む永続的なグリフ範囲配列も必要です。
`nf-md-cog`の`U+F0493`など`U+FFFF`を超える値には、ImGuiと利用側全体で
`IMGUI_USE_WCHAR32`を揃えてください。16 bitへ切り詰めてはいけません。
C++20の`u8`文字列は`char8_t[]`なので、ImGuiの`const char*`へ直接渡せません。
JSONの`utf8`から得たバイトを通常の文字列リテラルにします。後ろにラベルを
付けるときは`"\xEE\xAD\x91" " Settings"`と分けます。

[Dear ImGuiのフォントガイド](https://github.com/ocornut/imgui/blob/master/docs/FONTS.md)
に従い、最終UIでも確認してください。プレビューとImGuiではラスタライザ、
ヒンティング、DPI、レイアウトが異なる場合があります。

## インストールと配布

```sh
cmake --install build/release --config Release --prefix out/install
```

実行ファイルは自身の位置からデータを探します。ポータブル配置では実行ファイルの
隣の`assets`、インストール配置では`../share/findicon`を利用します。
`FINDICON_DATA_DIR`または`--data-dir`で変更できます。
インストールにはREADME、スキル、ライセンスも含まれます。配布時はこれらの
通知も保持してください。スキルのインストール先は
`share/findicon/skills/findicon`です。Codexで利用するには前述の検出先へコピーします。

システムのライブラリを使う場合は`-DFINDICON_FETCH_DEPS=OFF`を指定します。
FreeTypeとlibpngの開発パッケージが必要です。vcpkgではツールチェーンも指定し、
MSVCのランタイムを揃えます。既定の静的ランタイムには`x64-windows-static`、
`/MD`には`-DFINDICON_STATIC_RUNTIME=OFF`を使ってください。

## 終了値と出力

| 終了値 | 意味 |
| --- | --- |
| `0` | 成功 |
| `2` | 引数、カタログ、フォント、出力I/Oのエラー |
| `3` | 指定条件・ページに候補なし |
| `4` | `--strict`で欠落・非対応・クリップを検出。PNGとJSONは生成済み |

標準出力の既定形式はJSON、診断は標準エラーです。`--text`で短い表示にできます。
`search`・`info`・`families`は`--out`でJSONも保存できます。
空のプレビューは既存出力を上書きしません。入力ファイルと同じ出力先や
PNGとJSONの同一パスを拒否します。両ファイルを一時保存してから配置し、
通常の書き込み・rename失敗時は以前の組を保持または復元します。
電源断、強制終了、他プロセスの同時変更に対する二ファイル間の完全な原子性は
保証しないため、成功を確認してから出力を利用してください。

## テスト

```sh
cmake --preset test
cmake --build --preset test --parallel
ctest --preset test
```

Catch2とCLI統合テストで、検索、別名、ページ送り、Unicode、描画、PNG、
出力保護などを検証します。描画を変更した場合は生成PNGの目視確認も行います。

## 対象範囲・出典・ライセンス

対象は単一Unicodeグリフです。シェーピング、合字、双方向レイアウト、
複数コードポイントの絵文字、フォールバック、COLRv1 paint graphは対象外です。
CPU描画部分は[DokuText](https://github.com/poicurr/DokuText/tree/1db338c4ded4e76e54f4ceb46062ad2c632c900f)
から抽出しています。詳細は`thirdparty/DokuText/README.md`に記録しています。

同梱カタログとSymbols OnlyフォントはNerd Fonts 3.5.1です。
10,995個の名前、10,617個の異なるコードポイントを収録しています。
データとフォントは同じ版で更新してください。
`assets/Manifest.json`に出典URL、サイズ、SHA-256を記録しています。

**findicon本体・文書・findiconスキルは[MIT](LICENSE)です。フォントを含む
配布物全体がMITという意味ではありません。** Nerd FontsにはMIT、CC BY 4.0、
Apache 2.0、OFLなどの素材が含まれます。ラベル用DejaVu SansはBitstream Vera
ライセンスとDejaVu変更分のパブリックドメイン宣言に従います。
フォントやアイコンを再配布・利用する際は元の条件と通知を保持してください。
[第三者通知](THIRD_PARTY.md)に詳細と出典があります。
完全なMIT限定配布にするには、フォントとカタログを確認済みのMIT素材へ
置き換える必要があります。
