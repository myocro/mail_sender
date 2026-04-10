#pragma once

#include "mail_sender.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

struct SampleConfig {
  MailSender::Smtp smtp;
  std::string from_name;
  std::string from_address;
  std::vector<std::string> recipients;
  std::string subject;
  std::string body_text;

  void loadFromFile(const std::string& file_path) {
    const YAML::Node root = YAML::LoadFile(file_path);

    smtp.host = root["host"].as<std::string>();
    smtp.port = root["port"].as<int>();
    smtp.username = root["username"].as<std::string>();
    smtp.password = root["password"].as<std::string>();
    smtp.ehlo_host = root["ehlo_host"].as<std::string>();

    const std::string auth = toLower(root["auth"].as<std::string>());
    if (auth == "ssl") {
      smtp.auth = MailSender::Smtp::SSL_TLS;
    } else if (auth == "starttls") {
      smtp.auth = MailSender::Smtp::STARTTLS;
    } else {
      throw std::runtime_error("auth must be either 'ssl' or 'starttls'");
    }

    from_name = root["from_name"].as<std::string>();
    from_address = root["from_address"].as<std::string>();
    recipients = root["to"].as<std::vector<std::string>>();
    subject = root["subject"].as<std::string>();
    body_text = root["body"].as<std::string>();
  }

  std::string buildBodyHtml() const {
    return "<html><body><pre style=\"font-family: monospace; white-space: pre-wrap;\">"
           + htmlEscape(body_text) + "</pre></body></html>";
  }

 private:
  static std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
      return static_cast<char>(std::tolower(ch));
    });
    return value;
  }

  static std::string htmlEscape(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (char ch : text) {
      switch (ch) {
        case '&':
          escaped += "&amp;";
          break;
        case '<':
          escaped += "&lt;";
          break;
        case '>':
          escaped += "&gt;";
          break;
        case '"':
          escaped += "&quot;";
          break;
        case '\'':
          escaped += "&#39;";
          break;
        default:
          escaped.push_back(ch);
          break;
      }
    }
    return escaped;
  }
};
