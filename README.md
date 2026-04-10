# mail\_sender

内製IoTシステム向けに実装したお手軽メール送信ライブラリです。

- Linux, C++
- `OpenSSL`利用し `STARTTLS` と `SSL/TLS` に対応

y.myocro@gmail.com

## 依存

```bash
sudo apt install libssl-dev libyaml-cpp-dev
```

注：libyaml-cpp-devは、sampleコードでのyml読み込み用

## ビルド

```bash
./build.sh
```

成功すると `./sample_mail_sender` が生成されます。

## 実行

```bash
./sample_mail_sender sample_config.yml
```

## 設定ファイル

```yaml
host: smtp.example.com
port: 587
auth: starttls
username: user@example.com
password: your-password
ehlo_host: localhost
from_name: TEST
from_address: sender@example.com
to:
  - recipient@example.com
subject: mail_sender sample
body: |
  1行目
  2行目
  3行目
```

`auth` は `starttls` または `ssl` を指定

## 組み込み

CMake環境

```cmake
add_subdirectory(mail_sender/mail_sender)
```

## License

[MIT License](./LICENSE)
