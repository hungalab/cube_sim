# cube_sim
VMIPSをベースにしたCubeシステムのサイクルアキュレイトシミュレータ

## 特徴
Verilogシミュレーションではキャッシュのサイズやway数、メモリバンド幅など変更するのは容易ではないが、本シミュレーターはそれを可能にする。  
また、必要に応じて実行した命令のダンプや、各種レポートを表示可能である。  
C++で実装されているため、CADのライセンスなどは不要で、当然verilogシミュレーションよりも高速に実行できる。

# ビルド&実行
## 必要要件
1. git
1. gcc or clang
1. make

## ビルド
### 1. ソースの入手(本リポジトリ)
```
 $ git@github.com:hungalab/cube_sim.git
```

### 2. ビルド
```
 $ make
```
本リポジトリに含まれるMakefileではg++およびgccを用いる設定となっている。clang++およびclangを用いる場合はリポジトリ直下のMakefileとlibopcodes_mips/Makefileの変数を変更してください。
ビルドに成功すると実行ファイル `cube_sim` ができていると思います。

### 3. プログラムバイナリの用意  
Geyser用のプログラムコードをそのまま実行することが可能です。アプリケーション開発環境は[Cube2_TOPリポジトリ](https://github.com/hungalab/Cube2_TOP/)を利用してください。アプリケーションの作成方法についてもこのリポジトリのマニュアルを参照してください。  
VMIPSでは古い実行ファイル形式ECOFFでしたが、こちらのフローではELFにも対応しています。

下記pythonスクリプトを用いることで生成したアプリケーションhexファイルをバイナリに変換できます。
```
$ python3 test_vec/dump.py アプリケーションhexファイル 出力ファイル名
 ```

### 4. 実行
```
 $ ./cube_sim [-F vmipsrcファイル] [-o オプション] プログラムバイナリ
```
vmipsrcファイルは各種オプションをしているするファイルです。カレントディレクトリにこのファイルが存在すれば、引数に指定する必要はありません。

vmipsrcファイル以外にも`-o`オプションを用いてオプションを上書きすることができます。

例えば
```
 $ ./cube_sim -o instdump プログラムバイナリ
```
とすればinstdumpオプションが有効となります。

本シミュレータはbreak命令が実行されると停止するようになっているので、main関数終了直前に`__asm__("break 0x0")__;`などを挿入してアプリケーションをコンパイルしてください。

### GDBを使う
[wikiページ](https://github.com/hungalab/cube_sim/wiki/GDB%E3%82%92%E7%94%A8%E3%81%84%E3%81%9F%E3%83%87%E3%83%90%E3%83%83%E3%82%B0) を参照


## 利用可能なオプション
VMIPSから備わっているオプションに関しては[VMIPSのドキュメント](http://vmips.sourceforge.net/vmips/doc/vmips.html)を参照してください。

### cube_simから追加されたオプション
#### キャッシュ関連
* icacheway: 命令キャッシュのway数 (数値)
* icachebsize: 命令キャッシュブロックサイズ(バイト) (数値)
* icachebnum: 命令キャッシュのブロック数 (数値)
* dcacheway: データキャッシュのway数 (数値)
* dcachebsize: データキャッシュのブロック数 (数値)
* dcachebnum: データキャッシュのブロック数 (数値)
#### メモリアクセス関連
* mem_bandwidth: メモリバンド幅 (ワード数を指定する) (数値)
* bus_latency: バスアクセス権獲得メモリモジュールに要求が到達するまでのサイクル数 (数値)
* exmem_latency: 外部メモリにおける遅延サイクル数 (数値)

#### ルータ関連
* vcbufsize: virtual channelごとのバッファサイズ (数値)
* routermsg: ルータにおけるメッセージ表示有効化 (flag)
#### アクセラレータ
* accelerator0: 0番目のアクセラレータ (文字列)
* accelerator1: 1番目のアクセラレータ (文字列)
* accelerator2: 2番目のアクセラレータ (文字列)

#### SNACC用オプション
* snacc_sram_latency: SRAMからの読み出しレイテンシ(mad,madlp演算用)