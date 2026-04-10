#include "sample_config.hpp"

#include <iostream>

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " <config.yml>\n";
    return 1;
  }

  try {
    SampleConfig config;
    config.loadFromFile(argv[1]);

    MailSender sender;
    sender.smtp = config.smtp;
    sender.mail.from_name = config.from_name;
    sender.mail.from_address = config.from_address;
    sender.mail.rcpt = config.recipients;
    sender.mail.subject = config.subject;
    sender.mail.bodyHtml = config.buildBodyHtml();
    sender.setDebugMsgFunc([](const std::string& message) {
      std::cerr << message << '\n';
    });

    if (!sender.send()) {
      std::cerr << "mail send failed\n";
      return 1;
    }

    std::cout << "mail sent successfully\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
