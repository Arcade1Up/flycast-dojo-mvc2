#pragma once
#ifndef HISCORE_IPP_
#define HISCORE_IPP_

namespace sen {

struct HighScore {
  void initialize();
  void terminate();

  void readScores();
  void writeScores();
  void updateScores();

  std::array<uint64_t, 126> scores; // 分数
  std::array<uint64_t, 126> file_scores; // 分数
  bool is_read;

  static std::array<uint64_t, 126> default_scores;
};

extern HighScore highScore;

}

#endif //HISCORE_IPP_
