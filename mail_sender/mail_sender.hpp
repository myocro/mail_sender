// Copyright (c) 2026 myocro
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>

/*
  https://datatracker.ietf.org/doc/html/rfc2821
  https://datatracker.ietf.org/doc/html/rfc3207
*/
class MailSender {
public:
  MailSender();
  ~MailSender();

  struct Smtp {
    enum AuthenticationType {
      SSL_TLS,
      STARTTLS
    } auth = Smtp::STARTTLS;

    int port = 587;
    int timeout_milliseconds = 15000;
    std::string host;
    std::string username;
    std::string password;
    std::string ehlo_host = "localhost";
  } smtp;

  struct MailData {
    std::string from_address;
    std::string from_name;
    std::vector<std::string> rcpt;
    std::string subject;
    std::string bodyHtml;
    // 本文の言語 (RFC 3282 の言語タグ、例 "ja")。空なら Content-Language を出さない。
    // 受信側 (Gmail 等) は本文の中身から言語を推測することがあり、漢字中心で かな が
    // 少ない日本語の本文が中国語と判定される事例があったため、送信側から宣言できるようにした。
    // ⚠ あくまで宣言で、受信側がこれを優先する保証は無い
    std::string language;
  } mail;

  bool send();
  const std::string& lastError() const;
  void setDebugMsgFunc(std::function<void(const std::string&)> debugMsgFunc);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl;
};
