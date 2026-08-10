[EN](README.md) | [KO](README_KO.md)

# OctoPaint

OctoPaintはC++23で開発するWindowsネイティブ画像エディターです。最初のフロントエンドにはWinUI 3を使用しますが、エディターコア、アプリケーションAPI、レンダリング契約、ファイル形式の境界は、特定のUIフレームワークに依存しないように設計されています。

製品名、実行ファイル名、ウィンドウタイトルはすべて`OctoPaint`です。

## 計画している機能

- 複数ドキュメントの編集
- レイヤー、グループ、ブレンドモード、ラスターマスク、クリッピング関係
- コンポジット、RGB、アルファ、追加の名前付きチャンネル
- 選択範囲の操作と保存された選択範囲
- 明るさ/コントラスト、色相/彩度、カーブ、彩度除去の調整
- ガウスぼかしを含む画像フィルター
- クロップ、9方向の基準点によるキャンバスサイズ変更、比率またはピクセル単位の画像リサンプリング
- レイヤーを保持する独自の`.ocp`ドキュメント
- PNGおよびJPEGのインポート/エクスポート
- 互換性レポートを伴うレイヤー対応PSDのインポート/エクスポート

## アーキテクチャ

```text
OctoPaint.WinUI (交換可能なWinUI 3フロントエンド)
        |
        v
OctoPaint.Application (UI非依存のコマンドとスナップショット)
        |
        v
OctoPaint.Core (プラットフォーム非依存のドキュメントドメイン)
```

`OctoPaint.Core`はC++23標準ライブラリのみを使用します。WinUI、WinRT、Win32、Direct3D、およびその他のフロントエンド固有型は公開インターフェイスの外側に置きます。将来の別フロントエンドは、ドキュメントやエディターの動作を書き直すことなく`OctoPaint.Application`を利用できます。

## リポジトリ構成

- `src/OctoPaint.Core`: プラットフォーム非依存のドキュメントモデル
- `src/OctoPaint.Application`: UI非依存のコマンドと不変スナップショット
- `src/OctoPaint.WinUI`: 交換可能なWinUI 3アダプターと`OctoPaint`実行ファイル
- `tests`: Core、Application、ドメイン、ツール、レイヤー、合成、エディター状態、ペイント、選択を検証する9個のヘッドレステスト実行ファイル
- `docs`: アーキテクチャ、製品、エディター、ファイル形式の設計文書

## 設計文書

- [アーキテクチャとフロントエンド交換規則](docs/ARCHITECTURE.md)
- [製品要件](docs/PRODUCT_REQUIREMENTS.md)
- [エディターアーキテクチャ](docs/EDITOR_ARCHITECTURE.md)
- [ファイル形式と相互運用性](docs/FILE_FORMATS.md)

## ビルド

Visual Studio 2022で`OctoPaint.sln`を開いて`x64`プラットフォームを選択するか、次のコマンドを実行します。

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" .\OctoPaint.sln /restore /m /p:Configuration=Debug /p:Platform=x64
```

ビルド後、9個すべてのヘッドレス検証を次のように実行します。

```powershell
$tests = @(
  "OctoPaint.Core.Tests", "OctoPaint.Application.Tests", "OctoPaint.Core.Domain.Tests",
  "OctoPaint.Tools.Tests", "OctoPaint.Application.Layer.Tests", "OctoPaint.Application.Composite.Tests",
  "OctoPaint.Application.EditorState.Tests", "OctoPaint.Application.Paint.Tests", "OctoPaint.Application.Selection.Tests"
)
foreach ($test in $tests) {
  & ".\out\bin\x64\Debug\$test\$test.exe"
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
```

## リリースパッケージ

[WiX Toolset 5以降](https://docs.firegiant.com/wix/using-wix/)をインストールし、`VERSION`を`major.minor.patch`形式で更新してから次のファイルを実行します。

```bat
build-release.bat
```

スクリプトは依存関係を復元し、自己完結型のRelease x64アプリと9個のヘッドレステストをビルドし、それらのテストを実行してから次のファイルを生成します。

- `out\release\OctoPaint-<version>-win-x64.zip`
- `out\release\OctoPaint-<version>-win-x64.msi`

現時点では成果物の存在のみを確認します。パッケージ済みアプリの起動、ZIP内容の検査・再展開、MSIのインストール・アップグレード・修復・アンインストールは検証しません。

## プロジェクトの状態

現在のリポジトリには、動作するインメモリ編集の縦断スライスがあります。ドキュメントごとのUndo/Redoを備えた複数ドキュメント状態、疎タイルのRaster/Groupレイヤー、16種類のブレンドモードによるCPU合成、Pencil/Airbrush、replace方式の4種類の選択ツール、WinUI/D3Dキャンバスとレイヤーコントロールが実装されています。永続的なOpen/Saveとファイル形式、dirty-close保護、viewportナビゲーション、実際のMove Layerピクセル移動、および計画された編集機能の大半は未完成です。ソース監査に基づく状態と優先順位は[PROGRESS.md](PROGRESS.md)を参照してください。
