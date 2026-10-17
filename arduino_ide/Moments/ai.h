// Moments firmware - AI features (OpenAI-compatible chat completions, bring-your-own key).
#pragma once
#include "sources.h"

static String aiBase() {
  String b = cfg["ai"]["base"] | "https://api.groq.com/openai/v1";
  while (b.endsWith("/")) b.remove(b.length() - 1);
  return b;
}
static bool aiConfigured() { return secretHas("ai_key") || aiBase().startsWith("http://"); }

static String extractJson(const String& s) {
  int a = s.indexOf('{'), b = s.lastIndexOf('}');
  if (a < 0 || b <= a) return String();
  return s.substring(a, b + 1);
}

static bool aiChat(const String& sys, const String& user, String& content, int maxTokens, bool json) {
  String key;
  secretGet("ai_key", key);
  String base = aiBase();
  if (!key.length() && !base.startsWith("http://")) return false;
  JsonDocument req(&g_psAlloc);
  req["model"] = cfg["ai"]["model"] | "llama-3.3-70b-versatile";
  req["temperature"] = 0.7;
  req["max_tokens"] = maxTokens;
  if (json) req["response_format"]["type"] = "json_object";
  JsonArray msgs = req["messages"].to<JsonArray>();
  JsonObject m1 = msgs.add<JsonObject>();
  m1["role"] = "system";
  m1["content"] = sys;
  JsonObject m2 = msgs.add<JsonObject>();
  m2["role"] = "user";
  m2["content"] = user;
  String body;
  serializeJson(req, body);
  Headers h;
  if (key.length()) h.push_back({"Authorization", "Bearer " + key});
  String resp;
  int code = httpRequest("POST", base + "/chat/completions", body, "application/json", &h, &resp, 48000);
  if (code != 200) {
    LOGF("[ai] http %d %s\n", code, resp.substring(0, 160).c_str());
    return false;
  }
  JsonDocument d(&g_psAlloc);
  if (deserializeJson(d, resp)) return false;
  content = d["choices"][0]["message"]["content"] | "";
  if (json) content = extractJson(content);
  return content.length() > 0;
}

// ---------------------------------------------------------------------------
// Chef
static String mealNow(time_t t) {
  JsonObjectConst w = cfg["apps"]["chef"]["windows"];
  const char* names[4] = {"breakfast", "lunch", "dinner", "snack"};
  const char* defs[4] = {"05:00", "11:00", "15:00", "22:00"};
  int mod = modOf(t), best = -1, bestStart = -1, latest = -1, latestStart = -1;
  for (int i = 0; i < 4; i++) {
    int s = parseHHMM(w[names[i]] | defs[i], 0);
    if (s <= mod && s > bestStart) { best = i; bestStart = s; }
    if (s > latestStart) { latest = i; latestStart = s; }
  }
  return names[best >= 0 ? best : latest];
}
static uint32_t pantryHash() {
  String p, c;
  serializeJson(cfg["content"]["pantry"], p);
  serializeJson(cfg["apps"]["chef"], c);
  return fnvStr(c.c_str(), fnvStr(p.c_str()));
}
static String chefKey(time_t t) { return dateKey(t) + "/" + mealNow(t) + "/" + String(pantryHash(), HEX); }

static String joinArr(JsonArrayConst a, const char* sep = ", ") {
  String o;
  for (JsonVariantConst v : a) {
    if (o.length()) o += sep;
    o += v.as<const char*>();
  }
  return o;
}

static bool genChef() {
  time_t now = nowT();
  String meal = mealNow(now);
  JsonObjectConst c = cfg["apps"]["chef"];
  String sys =
      "You are a practical home-cooking assistant. Reply ONLY with JSON of the form "
      "{\"recipes\":[{\"title\":string,\"minutes\":int,\"servings\":int,\"uses\":[string],\"missing\":[string],"
      "\"steps\":[{\"text\":string,\"timer_seconds\":int}]}]}. "
      "Give 4 different recipes. 'uses' lists pantry items used, 'missing' lists ingredients the user must buy (keep it short). "
      "At most 8 steps per recipe, each step under 150 characters, written for a small screen. "
      "timer_seconds is 0 unless the step needs waiting (simmer, bake, rest). Plain ASCII text only.";
  String user = "Meal type: " + meal + ".\nPantry: " + joinArr(cfg["content"]["pantry"].as<JsonArrayConst>()) +
                ".\nDiet: " + jstr(c["diet"], "none") + ".\nAllergies (never use): " + jstr(c["allergies"], "none") +
                ".\nServings: " + String((int)(c["servings"] | 2)) + ".\nMax total time: " + String((int)(c["maxTime"] | 30)) +
                " minutes.\nPreferred cuisines: " + jstr(c["cuisines"], "any") + ".\nPrefer recipes that mostly use pantry items.";
  String out;
  if (!aiChat(sys, user, out, 3000, true)) return false;
  JsonDocument d(&g_psAlloc);
  if (deserializeJson(d, out)) return false;
  JsonDocument o(&g_psAlloc);
  o["meal"] = meal;
  o["date"] = dateKey(now);
  JsonArray rs = o["recipes"].to<JsonArray>();
  for (JsonObjectConst r : d["recipes"].as<JsonArrayConst>()) {
    if (rs.size() >= 5) break;
    String title = asciiFold(r["title"] | "");
    if (!title.length()) continue;
    JsonObject x = rs.add<JsonObject>();
    x["title"] = title.substring(0, 60);
    x["minutes"] = constrain((int)(r["minutes"] | 0), 0, 600);
    x["servings"] = constrain((int)(r["servings"] | 2), 1, 20);
    JsonArray u = x["uses"].to<JsonArray>();
    for (JsonVariantConst v : r["uses"].as<JsonArrayConst>()) if (u.size() < 12) u.add(asciiFold(v | "").substring(0, 40));
    JsonArray m = x["missing"].to<JsonArray>();
    for (JsonVariantConst v : r["missing"].as<JsonArrayConst>()) if (m.size() < 10) m.add(asciiFold(v | "").substring(0, 40));
    JsonArray st = x["steps"].to<JsonArray>();
    for (JsonObjectConst s : r["steps"].as<JsonArrayConst>()) {
      if (st.size() >= 10) break;
      JsonObject y = st.add<JsonObject>();
      y["text"] = asciiFold(s["text"] | "").substring(0, 220);
      y["timer"] = constrain((int)(s["timer_seconds"] | 0), 0, 4 * 3600);
    }
  }
  if (!rs.size()) return false;
  return writeJson("/cache/chef.json", o);
}

// ---------------------------------------------------------------------------
// Outfit suggestion
static JsonObjectConst wardrobeItem(const char* id) {
  for (JsonObjectConst it : cfg["content"]["wardrobe"].as<JsonArrayConst>())
    if (!strcmp(it["id"] | "", id)) return it;
  return JsonObjectConst();
}
static float toC(float t) { return unitsF() ? (t - 32) * 5.0f / 9.0f : t; }

static void ruleOutfit(JsonDocument& o) {
  o.clear();
  o["src"] = "rules";
  o["date"] = dateKey(nowT());
  JsonObjectConst d = wx()["daily"];
  float fmax = toC(d["apparent_temperature_max"][0] | 18.0f), fmin = toC(d["apparent_temperature_min"][0] | 12.0f);
  int rain = d["precipitation_probability_max"][0] | 0;
  float uv = d["uv_index_max"][0] | 0.0f;
  float avg = (fmax + fmin) / 2;
  int target = avg >= 24 ? 1 : avg >= 18 ? 2 : avg >= 12 ? 3 : avg >= 5 ? 4 : 5;
  bool wet = rain >= 40;
  JsonArray outs = o["outfits"].to<JsonArray>();
  JsonObject out = outs.add<JsonObject>();
  JsonArray items = out["items"].to<JsonArray>();
  auto pick = [&](const char* cat, int want, bool needRain) {
    const char* best = nullptr;
    int bestScore = 1000;
    for (JsonObjectConst it : cfg["content"]["wardrobe"].as<JsonArrayConst>()) {
      if (strcmp(it["cat"] | "", cat)) continue;
      int score = abs((int)(it["warm"] | 2) - want) * 10;
      if (needRain && !(it["rain"] | false)) score += 25;
      if (score < bestScore) { bestScore = score; best = it["id"] | ""; }
    }
    if (best && *best) items.add(best);
  };
  pick("top", min(target, 3), false);
  pick("bottom", target <= 1 ? 1 : 2, false);
  if (target >= 3 || wet) pick("outer", target, wet);
  pick("shoes", target >= 4 ? 3 : 2, wet);
  for (JsonObjectConst it : cfg["content"]["wardrobe"].as<JsonArrayConst>()) {
    if (strcmp(it["cat"] | "", "accessory")) continue;
    String n = it["name"] | "";
    n.toLowerCase();
    if ((wet && rain >= 50 && (it["rain"] | false)) || (uv > 6 && n.indexOf("sunglass") >= 0) || (target >= 5 && n.indexOf("scarf") >= 0))
      items.add(it["id"] | "");
  }
  char reason[96];
  snprintf(reason, sizeof(reason), "%d-%d%c, rain %d%%%s", (int)lroundf(d["apparent_temperature_min"][0] | 12.0f),
           (int)lroundf(d["apparent_temperature_max"][0] | 18.0f), 0x7F, rain, wet ? " - take rain gear" : (target >= 4 ? " - dress warm" : ""));
  out["reason"] = reason;
}

static bool genOutfit() {
  if (!wxOk()) return false;
  time_t now = nowT();
  String inv;
  for (JsonObjectConst it : cfg["content"]["wardrobe"].as<JsonArrayConst>()) {
    char b[160];
    snprintf(b, sizeof(b), "%s | %s | %s | warmth %d | rain-ok %s | formality %d | %s\n", (const char*)(it["id"] | ""),
             (const char*)(it["name"] | ""), (const char*)(it["cat"] | ""), (int)(it["warm"] | 2), (it["rain"] | false) ? "yes" : "no",
             (int)(it["form"] | 1), (const char*)(it["color"] | ""));
    inv += b;
  }
  if (!inv.length()) return false;
  String sys =
      "You are a helpful stylist. Reply ONLY with JSON: {\"outfits\":[{\"items\":[\"id\"],\"reason\":string}]}. "
      "Give 2 outfits using ONLY ids from the inventory: one top, one bottom, shoes, an outer layer if cool or rainy, "
      "and accessories only when useful (umbrella for rain, sunglasses for high UV). "
      "reason: under 80 characters, mention the temperature range and rain, plain ASCII.";
  String user = "Weather today: " + wxBrief() + "\nToday's calendar: " + agendaBrief(now, dayStart(now, 1), 6) +
                "\nInventory (id | name | category | warmth 1-5 | rain-ok | formality 1-5 | color):\n" + inv;
  String out;
  if (!aiChat(sys, user, out, 600, true)) return false;
  JsonDocument d(&g_psAlloc);
  if (deserializeJson(d, out)) return false;
  JsonDocument o(&g_psAlloc);
  o["src"] = "ai";
  o["date"] = dateKey(now);
  JsonArray outs = o["outfits"].to<JsonArray>();
  for (JsonObjectConst of : d["outfits"].as<JsonArrayConst>()) {
    if (outs.size() >= 3) break;
    JsonObject x = outs.add<JsonObject>();
    JsonArray items = x["items"].to<JsonArray>();
    for (JsonVariantConst id : of["items"].as<JsonArrayConst>()) {
      const char* s = id | "";
      if (!wardrobeItem(s).isNull() && items.size() < 7) items.add(s);
    }
    x["reason"] = asciiFold(of["reason"] | "").substring(0, 90);
    if (!items.size()) outs.remove(outs.size() - 1);
  }
  if (!outs.size()) return false;
  return writeJson("/cache/outfit.json", o);
}

// ---------------------------------------------------------------------------
// Summaries & custom AI text
static bool genNewsSummary(int feed) {
  String h = headlinesBrief(feed, 6);
  if (!h.length()) return false;
  String out;
  if (!aiChat("You summarize news headlines for a small e-paper display. Neutral tone, plain ASCII, no lists, no preamble.",
              "Summarize these headlines in 2 short sentences, at most 260 characters:\n" + h, out, 200, false))
    return false;
  JsonDocument o(&g_psAlloc);
  o["text"] = asciiFold(out.c_str()).substring(0, 320);
  return writeJson(cachePath("ai_news:" + String(feed)), o);
}
static bool genBriefing() {
  time_t now = nowT();
  String user = "Date: " + fmtDate(now, 0) + "\nWeather: " + wxBrief() + "\nCalendar today: " + agendaBrief(now, dayStart(now, 1), 6) +
                "\nHeadlines:\n" + headlinesBrief(0, 3) +
                "\nWrite a friendly daily briefing in at most 3 short sentences (max 300 characters). Plain ASCII.";
  String name = cfg["profile"]["name"] | "";
  String out;
  if (!aiChat("You write concise, warm daily briefings for " + (name.length() ? name : String("the user")) + ".", user, out, 220, false))
    return false;
  JsonDocument o(&g_psAlloc);
  o["text"] = asciiFold(out.c_str()).substring(0, 340);
  return writeJson("/cache/ai_brief.json", o);
}
static String fillAiVars(String p) {
  time_t now = nowT();
  p.replace("{weather}", wxBrief());
  p.replace("{agenda}", agendaBrief(now, dayStart(now, 1), 6));
  p.replace("{date}", fmtDate(now, 0));
  p.replace("{name}", cfg["profile"]["name"] | "");
  return p;
}
static bool genAiText(const String& wid, JsonObjectConst s) {
  String prompt = jstr(s["prompt"]);
  if (!prompt.length()) return false;
  String out;
  if (!aiChat("You write very short texts for a small e-paper display. Plain ASCII, no markdown, max 280 characters.",
              fillAiVars(prompt), out, 220, false))
    return false;
  JsonDocument o(&g_psAlloc);
  o["text"] = asciiFold(out.c_str()).substring(0, 320);
  return writeJson(cachePath("ai:" + wid), o);
}
