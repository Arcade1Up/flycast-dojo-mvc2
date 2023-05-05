#pragma once
#ifndef CONFIG_IPP_
#define CONFIG_IPP_

namespace sen {

struct Configs {
  auto initialize() -> void;
  auto parseArgs(int argc, char **argv) -> void;
  auto terminate() -> void;

  std::unique_ptr<cxxopts::Options> options_;
  cxxopts::ParseResult parse_result_;
};

extern Configs configs;
extern bool game_dip_changed;

}

#endif //CONFIG_IPP_
