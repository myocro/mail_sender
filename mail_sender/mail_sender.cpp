// Copyright (c) 2026 myocro
// SPDX-License-Identifier: MIT

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cstring>
#include <ctime>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "mail_sender.hpp"

namespace {

using Base64Byte = unsigned char;

const std::string kBase64Chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

bool is_base64(Base64Byte value) {
  return std::isalnum(value) || value == '+' || value == '/';
}

std::string base64_encode(const Base64Byte* buffer, unsigned int length) {
  std::string encoded;
  int i = 0;
  int j = 0;
  Base64Byte char_array_3[3];
  Base64Byte char_array_4[4];

  while (length--) {
    char_array_3[i++] = *(buffer++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] =
          ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] =
          ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for (i = 0; i < 4; ++i) {
        encoded += kBase64Chars[char_array_4[i]];
      }
      i = 0;
    }
  }

  if (i != 0) {
    for (j = i; j < 3; ++j) {
      char_array_3[j] = '\0';
    }

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] =
        ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] =
        ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (j = 0; j < i + 1; ++j) {
      encoded += kBase64Chars[char_array_4[j]];
    }

    while (i++ < 3) {
      encoded += '=';
    }
  }

  return encoded;
}

std::string base64_encode(const std::string& text) {
  return base64_encode(reinterpret_cast<const Base64Byte*>(text.data()),
                       static_cast<unsigned int>(text.size()));
}

std::string encode_utf8_base64(const std::string& text) {
  return "=?UTF-8?B?" + base64_encode(text) + "?=";
}

std::string fold_base64(const std::string& text, std::size_t width = 76) {
  std::string folded;
  for (std::size_t offset = 0; offset < text.size(); offset += width) {
    folded += text.substr(offset, width);
    folded += "\r\n";
  }
  return folded;
}

std::string dot_stuff(const std::string& text) {
  std::string stuffed;
  stuffed.reserve(text.size());
  bool line_start = true;
  for (char ch : text) {
    if (line_start && ch == '.') {
      stuffed.push_back('.');
    }
    stuffed.push_back(ch);
    if (ch == '\n') {
      line_start = true;
    } else if (ch != '\r') {
      line_start = false;
    }
  }
  return stuffed;
}

std::string build_smtp_data_payload(const std::string& message) {
  std::string payload = dot_stuff(message);
  if (payload.size() < 2 || payload.substr(payload.size() - 2) != "\r\n") {
    payload += "\r\n";
  }
  payload += ".\r\n";
  return payload;
}

std::string to_crlf(const std::string& text) {
  std::string converted;
  converted.reserve(text.size() + text.size() / 8);
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    if (ch == '\r') {
      converted.push_back('\r');
      if (i + 1 >= text.size() || text[i + 1] != '\n') {
        converted.push_back('\n');
      }
      continue;
    }
    if (ch == '\n') {
      converted.push_back('\r');
      converted.push_back('\n');
      continue;
    }
    converted.push_back(ch);
  }
  return converted;
}

bool parse_response_code(const std::string& line, int& response_code, bool& finished) {
  if (line.size() < 4) {
    return false;
  }
  if (!std::isdigit(line[0]) || !std::isdigit(line[1]) || !std::isdigit(line[2])) {
    return false;
  }
  response_code = (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
  finished = line[3] == ' ';
  return true;
}

std::string format_address(const std::string& address, const std::string& display_name) {
  if (display_name.empty()) {
    return "<" + address + ">";
  }
  return encode_utf8_base64(display_name) + " <" + address + ">";
}

std::string join_recipients(const std::vector<std::string>& recipients) {
  std::ostringstream stream;
  for (std::size_t index = 0; index < recipients.size(); ++index) {
    if (index != 0) {
      stream << ", ";
    }
    stream << "<" << recipients[index] << ">";
  }
  return stream.str();
}

std::string rfc2822_date() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
  std::tm local_tm {};
  localtime_r(&timestamp, &local_tm);

  char buffer[64];
  std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S %z", &local_tm);
  return buffer;
}

bool is_expected_code(int code, const std::vector<int>& expected_codes) {
  return std::find(expected_codes.begin(), expected_codes.end(), code) != expected_codes.end();
}

std::string join_lines(const std::vector<std::string>& lines, const std::string& delimiter) {
  std::ostringstream stream;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (i != 0) {
      stream << delimiter;
    }
    stream << lines[i];
  }
  return stream.str();
}

std::string to_upper(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
    return static_cast<char>(std::toupper(ch));
  });
  return text;
}

bool ehlo_supports(const std::vector<std::string>& lines, const std::string& capability) {
  const std::string needle = to_upper(capability);
  for (const std::string& line : lines) {
    if (line.size() < 4) {
      continue;
    }
    const std::string upper = to_upper(line.substr(4));
    if (upper == needle || upper.rfind(needle + " ", 0) == 0) {
      return true;
    }
    if (needle == "AUTH LOGIN" && upper.rfind("AUTH ", 0) == 0 &&
        upper.find("LOGIN") != std::string::npos) {
      return true;
    }
  }
  return false;
}

class SslInitializer {
 public:
  SslInitializer() {
    SSL_library_init();
    SSL_load_error_strings();
    OPENSSL_init_ssl(0, nullptr);
  }
};

SslInitializer& ssl_initializer() {
  static SslInitializer instance;
  return instance;
}

class SockIO {
 public:
  ~SockIO() { close(); }

  bool connect(const std::string& host, int port, int timeout_milliseconds) {
    close();
    debug_stream.str("");
    debug_stream.clear();
    host_ = host;
    timeout_milliseconds_ = std::max(timeout_milliseconds, 1);

    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* results = nullptr;
    const std::string service = std::to_string(port);
    const int lookup = getaddrinfo(host.c_str(), service.c_str(), &hints, &results);
    if (lookup != 0) {
      debug_stream << "getaddrinfo failed: " << gai_strerror(lookup);
      return false;
    }

    bool connected = false;
    for (struct addrinfo* candidate = results; candidate != nullptr;
         candidate = candidate->ai_next) {
      socket_fd_ = ::socket(candidate->ai_family, candidate->ai_socktype,
                            candidate->ai_protocol);
      if (socket_fd_ < 0) {
        continue;
      }

      const int flags = fcntl(socket_fd_, F_GETFL, 0);
      if (flags < 0 || fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        const int saved_errno = errno;
        ::close(socket_fd_);
        socket_fd_ = -1;
        debug_stream << "fcntl failed: " << std::strerror(saved_errno) << '\n';
        continue;
      }

      const int connect_result = ::connect(socket_fd_, candidate->ai_addr, candidate->ai_addrlen);
      if (connect_result == 0) {
        fcntl(socket_fd_, F_SETFL, flags);
        connected = true;
        break;
      }

      if (errno == EINPROGRESS) {
        struct pollfd poll_fd {};
        poll_fd.fd = socket_fd_;
        poll_fd.events = POLLOUT;
        const int poll_result = poll(&poll_fd, 1, timeout_milliseconds_);
        if (poll_result > 0 && (poll_fd.revents & POLLOUT) != 0) {
          int socket_error = 0;
          socklen_t length = sizeof(socket_error);
          if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &socket_error, &length) == 0 &&
              socket_error == 0) {
            fcntl(socket_fd_, F_SETFL, flags);
            connected = true;
            break;
          }
          const int saved_errno = socket_error == 0 ? errno : socket_error;
          ::close(socket_fd_);
          socket_fd_ = -1;
          debug_stream << "connect failed: " << std::strerror(saved_errno) << '\n';
          continue;
        }
        if (poll_result == 0) {
          ::close(socket_fd_);
          socket_fd_ = -1;
          debug_stream << "connect timeout after " << timeout_milliseconds_ << " ms\n";
          continue;
        }
      }

      const int saved_errno = errno;
      ::close(socket_fd_);
      socket_fd_ = -1;
      debug_stream << "connect failed: " << std::strerror(saved_errno) << '\n';
    }

    freeaddrinfo(results);

    if (!connected) {
      debug_stream << "all connection attempts failed";
      return false;
    }

    struct timeval timeout {};
    timeout.tv_sec = timeout_milliseconds_ / 1000;
    timeout.tv_usec = static_cast<suseconds_t>((timeout_milliseconds_ % 1000) * 1000);
    setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(socket_fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    return true;
  }

  bool start_ssl() {
    if (socket_fd_ < 0) {
      debug_stream << "socket is not connected";
      return false;
    }

    ssl_initializer();
    ssl_context_.reset(SSL_CTX_new(TLS_client_method()));
    if (!ssl_context_) {
      append_ssl_error("SSL_CTX_new failed");
      return false;
    }

    SSL_CTX_set_default_verify_paths(ssl_context_.get());
    SSL_CTX_set_verify(ssl_context_.get(), SSL_VERIFY_PEER, nullptr);

    ssl_.reset(SSL_new(ssl_context_.get()));
    if (!ssl_) {
      append_ssl_error("SSL_new failed");
      return false;
    }

    SSL_set_tlsext_host_name(ssl_.get(), host_.c_str());
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    SSL_set1_host(ssl_.get(), host_.c_str());
#endif
    SSL_set_fd(ssl_.get(), socket_fd_);

    if (SSL_connect(ssl_.get()) != 1) {
      append_ssl_error("SSL_connect failed");
      return false;
    }
    if (SSL_get_verify_result(ssl_.get()) != X509_V_OK) {
      debug_stream << "TLS certificate verification failed";
      return false;
    }
    return true;
  }

  bool write_all(const std::string& data) {
    std::size_t written = 0;
    while (written < data.size()) {
      const int result = ssl_
                             ? SSL_write(ssl_.get(), data.data() + written,
                                         static_cast<int>(data.size() - written))
                             : static_cast<int>(::send(socket_fd_, data.data() + written,
                                                       data.size() - written, 0));
      if (result <= 0) {
        if (ssl_) {
          append_ssl_error("write failed");
        } else {
          debug_stream << "write failed: " << std::strerror(errno);
        }
        return false;
      }
      written += static_cast<std::size_t>(result);
    }
    return true;
  }

  bool read_response(std::vector<std::string>& lines) {
    lines.clear();
    for (;;) {
      std::size_t line_end = pending_.find("\r\n");
      if (line_end != std::string::npos) {
        std::string line = pending_.substr(0, line_end);
        pending_.erase(0, line_end + 2);
        lines.push_back(line);

        int response_code = 0;
        bool finished = false;
        if (parse_response_code(line, response_code, finished) && finished) {
          return true;
        }
        continue;
      }

      char buffer[4096];
      const int result =
          ssl_ ? SSL_read(ssl_.get(), buffer, sizeof(buffer))
               : static_cast<int>(::recv(socket_fd_, buffer, sizeof(buffer), 0));
      if (result <= 0) {
        if (ssl_) {
          append_ssl_error("read failed");
        } else {
          debug_stream << "read failed: " << std::strerror(errno);
        }
        return false;
      }
      pending_.append(buffer, static_cast<std::size_t>(result));
    }
  }

  std::string debug_text() const { return debug_stream.str(); }

  void close() {
    pending_.clear();
    if (ssl_) {
      SSL_shutdown(ssl_.get());
      ssl_.reset();
    }
    ssl_context_.reset();
    if (socket_fd_ >= 0) {
      ::close(socket_fd_);
      socket_fd_ = -1;
    }
  }

 private:
  struct SslContextDeleter {
    void operator()(SSL_CTX* pointer) const {
      if (pointer != nullptr) {
        SSL_CTX_free(pointer);
      }
    }
  };

  struct SslDeleter {
    void operator()(SSL* pointer) const {
      if (pointer != nullptr) {
        SSL_free(pointer);
      }
    }
  };

  void append_ssl_error(const std::string& prefix) {
    char error_buffer[256];
    const unsigned long error = ERR_get_error();
    if (error == 0) {
      debug_stream << prefix;
      return;
    }
    ERR_error_string_n(error, error_buffer, sizeof(error_buffer));
    debug_stream << prefix << ": " << error_buffer;
  }

  std::string host_;
  int timeout_milliseconds_ = 15000;
  int socket_fd_ = -1;
  std::unique_ptr<SSL_CTX, SslContextDeleter> ssl_context_;
  std::unique_ptr<SSL, SslDeleter> ssl_;
  std::string pending_;
  std::ostringstream debug_stream;
};

}  // namespace

class MailSender::Impl {
 public:
  struct Command {
    std::string payload;
    bool hide_in_log = false;
    bool append_crlf = true;
    std::vector<int> expected_codes;
  };

  void print(const std::string& message) const {
    if (debug_callback) {
      debug_callback(message);
    }
  }

  void print_lines(const std::vector<std::string>& lines) const {
    for (const auto& line : lines) {
      print("S: " + line);
    }
  }

  void set_error(std::string message) {
    last_error = std::move(message);
    print("ERROR: " + last_error);
  }

  bool send_command(const Command& command) {
    const std::string wire =
        command.append_crlf ? command.payload + "\r\n" : command.payload;
    if (!socket.write_all(wire)) {
      set_error(socket.debug_text());
      return false;
    }

    print(command.hide_in_log ? "C: -----------" : "C: " + command.payload);

    last_response_lines.clear();
    if (!socket.read_response(last_response_lines)) {
      set_error(socket.debug_text());
      return false;
    }
    print_lines(last_response_lines);

    int response_code = 0;
    bool finished = false;
    if (last_response_lines.empty() ||
        !parse_response_code(last_response_lines.back(), response_code, finished) ||
        !is_expected_code(response_code, command.expected_codes)) {
      std::ostringstream stream;
      stream << "unexpected SMTP response";
      if (!last_response_lines.empty()) {
        stream << ": " << join_lines(last_response_lines, " | ");
      }
      set_error(stream.str());
      return false;
    }
    last_response_code = response_code;
    return true;
  }

  SockIO socket;
  std::function<void(const std::string&)> debug_callback;
  int last_response_code = 0;
  std::vector<std::string> last_response_lines;
  std::string last_error;
};

MailSender::MailSender() : pimpl(std::make_unique<Impl>()) {}

MailSender::~MailSender() = default;

const std::string& MailSender::lastError() const {
  return pimpl->last_error;
}

void MailSender::setDebugMsgFunc(
    std::function<void(const std::string&)> debugMsgFunc) {
  pimpl->debug_callback = std::move(debugMsgFunc);
}

bool MailSender::send() {
  pimpl->last_error.clear();
  if (smtp.host.empty() || smtp.username.empty() || smtp.password.empty() ||
      mail.from_address.empty() || mail.rcpt.empty() || mail.bodyHtml.empty()) {
    pimpl->set_error("required SMTP or mail fields are empty");
    return false;
  }
  if (smtp.timeout_milliseconds <= 0) {
    pimpl->set_error("smtp.timeout_milliseconds must be greater than zero");
    return false;
  }

  pimpl->print("Connect to " + smtp.host + ":" + std::to_string(smtp.port));
  if (!pimpl->socket.connect(smtp.host, smtp.port, smtp.timeout_milliseconds)) {
    pimpl->set_error(pimpl->socket.debug_text());
    return false;
  }

  std::vector<std::string> banner_lines;
  if (!pimpl->socket.read_response(banner_lines)) {
    pimpl->set_error(pimpl->socket.debug_text());
    return false;
  }
  pimpl->print_lines(banner_lines);

  int banner_code = 0;
  bool finished = false;
  if (!parse_response_code(banner_lines.back(), banner_code, finished) ||
      banner_code != 220) {
    pimpl->set_error("invalid SMTP banner: " + join_lines(banner_lines, " | "));
    return false;
  }

  if (smtp.auth == Smtp::SSL_TLS) {
    if (!pimpl->socket.start_ssl()) {
      pimpl->set_error(pimpl->socket.debug_text());
      return false;
    }
    pimpl->print("--- SSL/TLS connected ---");
  } else if (smtp.auth == Smtp::STARTTLS) {
    if (!pimpl->send_command({"EHLO " + smtp.ehlo_host, false, true, {250}})) {
      return false;
    }
    if (!ehlo_supports(pimpl->last_response_lines, "STARTTLS")) {
      pimpl->set_error("server does not advertise STARTTLS");
      return false;
    }
    if (!pimpl->send_command({"STARTTLS", false, true, {220}})) {
      return false;
    }
    if (!pimpl->socket.start_ssl()) {
      pimpl->set_error(pimpl->socket.debug_text());
      return false;
    }
    pimpl->print("--- STARTTLS connected ---");
  } else {
    pimpl->set_error("unsupported SMTP authentication mode");
    return false;
  }

  if (!pimpl->send_command({"EHLO " + smtp.ehlo_host, false, true, {250}})) {
    return false;
  }
  if (!ehlo_supports(pimpl->last_response_lines, "AUTH LOGIN")) {
    pimpl->set_error("server does not advertise AUTH LOGIN");
    return false;
  }

  std::vector<Impl::Command> commands = {
      {"AUTH LOGIN", false, true, {334}},
      {base64_encode(smtp.username), true, true, {334}},
      {base64_encode(smtp.password), true, true, {235}},
      {"MAIL FROM:<" + mail.from_address + ">", false, true, {250}}};

  for (const auto& recipient : mail.rcpt) {
    commands.push_back({"RCPT TO:<" + recipient + ">", false, true, {250, 251}});
  }
  commands.push_back({"DATA", false, true, {354}});

  std::string message;
  message += "Date: " + rfc2822_date() + "\r\n";
  message += "From: " + format_address(mail.from_address, mail.from_name) + "\r\n";
  message += "To: " + join_recipients(mail.rcpt) + "\r\n";
  message += "Subject: " + encode_utf8_base64(mail.subject) + "\r\n";
  message += "MIME-Version: 1.0\r\n";
  message += "Content-Type: text/html; charset=UTF-8\r\n";
  message += "Content-Transfer-Encoding: base64\r\n";
  message += "\r\n";
  message += fold_base64(base64_encode(to_crlf(mail.bodyHtml)));

  for (const auto& command : commands) {
    if (!pimpl->send_command(command)) {
      return false;
    }
  }

  if (!pimpl->send_command({build_smtp_data_payload(message), false, false, {250}})) {
    return false;
  }

  pimpl->send_command({"QUIT", false, true, {221}});
  return true;
}
