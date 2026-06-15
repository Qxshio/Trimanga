#include "trimanga/classifier.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <numeric>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>

namespace trimanga {

namespace {

const std::unordered_set<std::string> kRoleWords = {
    "raw", "raws", "translator", "translation", "translating", "cleaner", "cleaning",
    "typesetter", "typesetting", "proofreader", "proofreading", "redrawer", "redrawing",
    "scanner", "scanning", "letterer", "lettering", "editor", "editing", "qc", "qcer"};

const std::unordered_set<std::string> kSocialWords = {
    "discord", "patreon", "kofi", "ko-fi", "twitter", "instagram", "tumblr", "facebook", "reddit"};

const std::unordered_set<std::string> kSupportWords = {
    "support", "buy", "purchase", "purchasing", "subscribe", "donate", "donation"};

const std::unordered_set<std::string> kOfficialWords = {
    "official", "licensed", "publisher", "artist", "author", "creator", "release", "country"};

bool contains_any(const std::set<std::string>& words, const std::unordered_set<std::string>& needles) {
  for (const auto& word : words) {
    if (needles.contains(word)) {
      return true;
    }
  }
  return false;
}

int bounded_edit_distance(const std::string& left, const std::string& right, int limit) {
  if (std::abs(static_cast<int>(left.size()) - static_cast<int>(right.size())) > limit) {
    return limit + 1;
  }
  std::vector<int> previous(right.size() + 1);
  std::iota(previous.begin(), previous.end(), 0);
  for (std::size_t i = 0; i < left.size(); ++i) {
    std::vector<int> current(right.size() + 1);
    current[0] = static_cast<int>(i + 1);
    int row_min = current[0];
    for (std::size_t j = 0; j < right.size(); ++j) {
      const int cost = left[i] == right[j] ? 0 : 1;
      current[j + 1] = std::min({previous[j + 1] + 1, current[j] + 1, previous[j] + cost});
      row_min = std::min(row_min, current[j + 1]);
    }
    if (row_min > limit) {
      return limit + 1;
    }
    previous = std::move(current);
  }
  return previous.back();
}

bool starts_with(const std::string& value, const std::string& prefix) {
  return value.rfind(prefix, 0) == 0;
}

bool word_is_scanish(const std::string& word) {
  return starts_with(word, "scan") || bounded_edit_distance(word, "scans", 1) <= 1 ||
         (word.size() >= 5 && bounded_edit_distance(word, "scans", 2) <= 2) ||
         bounded_edit_distance(word, "scanlation", 3) <= 3 ||
         bounded_edit_distance(word, "seans", 1) <= 1 || bounded_edit_distance(word, "seons", 1) <= 1;
}

int staff_role_word_count(const std::set<std::string>& words) {
  const std::vector<std::string> roots = {
      "raw",      "translat", "trens",   "mranslat", "liauislat", "transiator",
      "clean",    "typeset",  "lypeset", "typess",   "tupes",     "tepese",
      "proofread", "froot",   "oofread", "redraw",   "letter",    "edit",
      "quality",  "qc"};
  int count = 0;
  for (const std::string& word : words) {
    for (const std::string& root : roots) {
      if (starts_with(word, root) || (word.size() >= root.size() + 1 && word.find(root) != std::string::npos)) {
        ++count;
        break;
      }
    }
  }
  return count;
}

bool has_role_label_cluster(const std::string& normalized) {
  static const std::regex pattern(
      R"(\b(raws?|translation|translator|clean(?:ing|er)?|typesett(?:ing|er)?|proofread(?:ing|er)?|redraw(?:ing|er)?|letter(?:ing|er)?|edit(?:ing|or)?|qc|qcer)\b\s*[:：-])",
      std::regex::icase);
  int count = 0;
  for (std::sregex_iterator it(normalized.begin(), normalized.end(), pattern), end; it != end; ++it) {
    ++count;
  }
  return count >= 2;
}

bool has_explicit_contact_signal(const std::string& normalized) {
  static const std::regex pattern(R"(([\w.+-]+@[\w.-]+\.[a-z]{2,})|(https?://|www\.|\.com\b|\.org\b|\.net\b))",
                                  std::regex::icase);
  return std::regex_search(normalized, pattern);
}

bool has_contact_signal(const std::string& normalized, const std::set<std::string>& words) {
  if (has_explicit_contact_signal(normalized) || words.contains("contact")) {
    return true;
  }
  return std::any_of(words.begin(), words.end(), [](const std::string& word) {
    return bounded_edit_distance(word, "contact", 2) <= 2;
  });
}

bool has_recruitment_signal(const std::set<std::string>& words) {
  bool recruit = false;
  for (const std::string& word : words) {
    if (starts_with(word, "recru")) {
      recruit = true;
      break;
    }
  }
  return recruit && (staff_role_word_count(words) > 0 || words.contains("discord") || words.contains("team") || words.contains("group"));
}

bool has_support_or_official_ad(const std::set<std::string>& words) {
  return contains_any(words, kSupportWords) && contains_any(words, kOfficialWords);
}

bool has_scan_group_footer(const std::string& normalized) {
  const std::size_t start = normalized.size() > 260 ? normalized.size() - 260 : 0;
  const std::vector<std::string> tail_words = text_words(normalized.substr(start));
  for (std::size_t index = 0; index < tail_words.size(); ++index) {
    if (!word_is_scanish(tail_words[index])) {
      continue;
    }
    const std::size_t begin = index > 5 ? index - 5 : 0;
    for (std::size_t look = begin; look < index; ++look) {
      if (tail_words[look] != "the" && tail_words[look] != "and" && tail_words[look] != "for" && tail_words[look] != "you") {
        return true;
      }
    }
  }
  return false;
}

bool has_scan_group_present_signal(const std::string& normalized, const std::set<std::string>& words) {
  const std::vector<std::string> all_words = text_words(normalized);
  const bool scan_word = std::any_of(all_words.begin(), all_words.end(), word_is_scanish);
  if (!scan_word) {
    return false;
  }
  const bool present_word = std::any_of(all_words.begin(), all_words.end(), [](const std::string& word) {
    return bounded_edit_distance(word, "presents", 3) <= 3 || bounded_edit_distance(word, "present", 3) <= 3;
  });
  if (!present_word) {
    return false;
  }
  return words.contains("chapter") || words.contains("credits") || words.contains("credit") || words.contains("translator") ||
         words.contains("cleaner") || words.contains("typesetter") || words.contains("quality") || words.contains("visit") ||
         has_explicit_contact_signal(normalized);
}

int indicator_score(const std::string& normalized, const std::set<std::string>& words) {
  int score = 0;
  const int role_count = std::max(static_cast<int>(std::count_if(kRoleWords.begin(), kRoleWords.end(), [&](const std::string& word) {
                                  return words.contains(word);
                                })),
                                staff_role_word_count(words));
  if (has_role_label_cluster(normalized) || role_count >= 4) {
    score += 8;
  } else if (role_count >= 3) {
    score += 5;
  }
  if (words.contains("credit") || words.contains("credits")) {
    score += 3;
  }
  if (has_support_or_official_ad(words)) {
    score += 7;
  }
  if (contains_any(words, kSocialWords) || has_explicit_contact_signal(normalized)) {
    score += 5;
  }
  if (has_recruitment_signal(words)) {
    score += 6;
  }
  if (has_contact_signal(normalized, words) && staff_role_word_count(words) > 0) {
    score += 5;
  }
  if (has_scan_group_present_signal(normalized, words)) {
    score += 8;
  }
  if (normalized.find("scanlation") != std::string::npos || normalized.find("scanlated") != std::string::npos) {
    score += 5;
  }
  return score;
}

double z(double value, const FeatureStats& stats) {
  return std::abs(value - stats.mean) / std::max(stats.stdev, 1e-9);
}

std::pair<double, bool> visual_outlier_score(const PageFeatures& features, const VolumeProfile& profile) {
  if (!features.valid || !profile.valid) {
    return {0.0, false};
  }
  const std::vector<double> scores = {
      z(features.aspect_ratio, profile.aspect_ratio),
      z(features.edge_density, profile.edge_density),
      z(features.entropy, profile.entropy),
      z(features.ink_coverage, profile.ink_coverage),
      z(features.dark_coverage, profile.dark_coverage),
      z(features.white_coverage, profile.white_coverage),
      z(features.artwork_coverage, profile.artwork_coverage),
      z(features.panel_count, profile.panel_count),
  };
  int strong = 0;
  bool extreme = false;
  double sum = 0.0;
  for (double value : scores) {
    strong += value >= 2.5 ? 1 : 0;
    extreme = extreme || value >= 4.0;
    sum += value * value;
  }
  const double distance = std::sqrt(sum / static_cast<double>(scores.size()));
  return {distance, extreme || distance >= 3.7 || strong >= 3 || (strong >= 2 && distance >= 2.8)};
}

double story_page_confidence(const PageFeatures& features, const VolumeProfile& profile, std::size_t word_count) {
  if (!features.valid || !profile.valid) {
    return 0.0;
  }
  double confidence = 0.0;
  if (features.edge_density >= profile.edge_density.median * 0.65) {
    confidence += 1.5;
  }
  if (features.artwork_coverage >= profile.artwork_coverage.median * 0.65) {
    confidence += 1.5;
  }
  if (features.entropy >= profile.entropy.median * 0.70) {
    confidence += 1.0;
  }
  if (features.panel_count >= 2.0) {
    confidence += 1.0;
  }
  if (word_count >= 8 && word_count <= 180) {
    confidence += 1.0;
  }
  if (word_count > 80 && features.artwork_coverage >= profile.artwork_coverage.median * 0.5) {
    confidence += 1.0;
  }
  return confidence;
}

bool has_compact_staff_credit_page(const std::string& normalized, const std::set<std::string>& words, std::size_t word_count) {
  return word_count <= 90 && (has_role_label_cluster(normalized) || staff_role_word_count(words) >= 4);
}

}  // namespace

std::string normalize_text(const std::string& text) {
  std::string out;
  bool space = false;
  for (unsigned char ch : text) {
    if (std::isspace(ch)) {
      if (!space && !out.empty()) {
        out += ' ';
      }
      space = true;
    } else {
      out += static_cast<char>(std::tolower(ch));
      space = false;
    }
  }
  return out;
}

std::vector<std::string> text_words(const std::string& text) {
  static const std::regex word_pattern(R"([a-z][a-z0-9-]{1,})");
  std::vector<std::string> words;
  for (std::sregex_iterator it(text.begin(), text.end(), word_pattern), end; it != end; ++it) {
    words.push_back(it->str());
  }
  return words;
}

Classification classify_page(const std::string& text, const PageFeatures& features, const VolumeProfile& profile) {
  const std::string normalized = normalize_text(text);
  const std::vector<std::string> word_vector = text_words(normalized);
  const std::set<std::string> words(word_vector.begin(), word_vector.end());
  auto [outlier_distance, extreme_outlier] = visual_outlier_score(features, profile);
  const double story = story_page_confidence(features, profile, word_vector.size());

  Classification result;
  result.story = story;
  result.outlier = outlier_distance;

  if (normalized.empty()) {
    result.score = (extreme_outlier ? outlier_distance * 2.0 : 0.0) - story * 3.0;
    result.candidate = extreme_outlier && story < 1.0 && outlier_distance >= 5.5 && result.score >= 8.0;
    return result;
  }

  const int indicator = indicator_score(normalized, words);
  result.indicator = indicator;
  result.score = indicator + (extreme_outlier ? outlier_distance * 2.0 : 0.0) - story * 3.0;

  const bool compact_staff = has_compact_staff_credit_page(normalized, words, word_vector.size());
  const bool role_cluster = has_role_label_cluster(normalized);
  const int role_count = staff_role_word_count(words);
  const bool credit_layout = role_cluster || (has_scan_group_footer(normalized) && role_count >= 2);
  const bool explicit_contact = has_explicit_contact_signal(normalized);
  const bool strong_ad_or_recruitment = has_support_or_official_ad(words) || has_recruitment_signal(words) ||
                                        (explicit_contact && role_count > 0);
  const bool visual_only_anomaly = indicator <= 0 && extreme_outlier && story < 1.0 && outlier_distance >= 5.5 &&
                                   result.score >= 8.0;

  if (indicator <= 0 && !visual_only_anomaly) {
    return result;
  }
  if ((compact_staff || credit_layout) && indicator >= 8) {
    result.candidate = true;
    result.visual_seed = true;
    return result;
  }
  if (story >= 5.0 && !strong_ad_or_recruitment && !compact_staff && indicator < 12) {
    return result;
  }
  if (story >= 4.0 && indicator < 8 && !extreme_outlier) {
    return result;
  }
  result.candidate = (indicator >= 7 && strong_ad_or_recruitment) ||
                     (indicator >= 10 && (compact_staff || strong_ad_or_recruitment || outlier_distance >= 2.5)) ||
                     (indicator >= 8 && outlier_distance >= 2.5) ||
                     (extreme_outlier && indicator >= 5 && result.score >= -5.0) || visual_only_anomaly;
  result.visual_seed = result.candidate &&
                       (compact_staff || credit_layout || strong_ad_or_recruitment || indicator >= 12 ||
                        (indicator >= 8 && story < 4.0));
  return result;
}

}  // namespace trimanga
