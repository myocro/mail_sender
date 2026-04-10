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
  } mail;

  bool send();
  const std::string& lastError() const;
  void setDebugMsgFunc(std::function<void(const std::string&)> debugMsgFunc);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl;
};
