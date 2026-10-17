// Moments firmware - configuration storage (LittleFS JSON), secrets (NVS) and defaults.
#pragma once
#include "gfx.h"
#include "quotes.h"

// Default configuration. Grid: portrait 6x10 cells, landscape 10x6 cells (80x80 px).
static const char DEFAULT_CONFIG[] PROGMEM = R"JSON({
"v":1,
"profile":{"name":"","units":"C","h24":true,"wake":["07:00","07:00","07:00","07:00","07:00","08:30","08:30"]},
"loc":{"lat":null,"lon":null,"city":"","tz":"UTC0"},
"wifi":[],
"feeds":[{"name":"BBC World","url":"https://feeds.bbci.co.uk/news/world/rss.xml"},{"name":"NPR News","url":"https://feeds.npr.org/1001/rss.xml"}],
"cals":[],
"stocks":{"provider":"twelvedata"},
"ai":{"base":"https://api.groq.com/openai/v1","model":"llama-3.3-70b-versatile"},
"notify":{"ntfy":true,"ntfyServer":"https://ntfy.sh","discord":false,"telegram":false,"tgChat":"","webhook":false},
"owners":[],
"dayparts":[
 {"id":"morning","name":"Morning","start":"05:00","end":"11:00"},
 {"id":"day","name":"Day","start":"11:00","end":"17:00"},
 {"id":"evening","name":"Evening","start":"17:00","end":"21:00"},
 {"id":"night","name":"Night","start":"21:00","end":"05:00"}],
"screens":[
 {"id":"morning","name":"Morning Briefing","o":"P","theme":"light","w":[
  {"id":"mb1","t":"greeting","x":0,"y":0,"w":6,"h":1,"s":{"date":true}},
  {"id":"mb2","t":"weather_now","x":0,"y":1,"w":6,"h":2,"s":{}},
  {"id":"mb3","t":"hourly","x":0,"y":3,"w":6,"h":2,"s":{"hours":12,"show":"both"}},
  {"id":"mb4","t":"agenda","x":0,"y":5,"w":6,"h":2,"s":{"range":"today","max":4}},
  {"id":"mb5","t":"news_summary","x":0,"y":7,"w":6,"h":2,"s":{"feed":0}},
  {"id":"mb6","t":"indoor","x":0,"y":9,"w":3,"h":1,"s":{}},
  {"id":"mb7","t":"status","x":3,"y":9,"w":3,"h":1,"s":{}}]},
 {"id":"night","name":"Night Briefing","o":"P","theme":"dark","w":[
  {"id":"nb1","t":"clock","x":0,"y":0,"w":6,"h":2,"s":{"style":"big","date":true}},
  {"id":"nb2","t":"sleep_calc","x":0,"y":2,"w":6,"h":2,"s":{"fall":15}},
  {"id":"nb3","t":"agenda","x":0,"y":4,"w":6,"h":2,"s":{"range":"tomorrow","max":3}},
  {"id":"nb4","t":"daily","x":0,"y":6,"w":6,"h":1,"s":{"days":1,"tomorrow":true}},
  {"id":"nb5","t":"quote","x":0,"y":7,"w":6,"h":3,"s":{}}]},
 {"id":"bedside","name":"Bedside Glance","o":"P","theme":"light","w":[
  {"id":"bg1","t":"clock","x":0,"y":0,"w":6,"h":3,"s":{"style":"big","date":false}},
  {"id":"bg2","t":"date","x":0,"y":3,"w":6,"h":1,"s":{}},
  {"id":"bg3","t":"weather_now","x":0,"y":4,"w":6,"h":3,"s":{}},
  {"id":"bg4","t":"next_event","x":0,"y":7,"w":6,"h":2,"s":{}},
  {"id":"bg5","t":"status","x":0,"y":9,"w":6,"h":1,"s":{}}]},
 {"id":"desk","name":"Desk","o":"L","theme":"light","w":[
  {"id":"dk1","t":"clock","x":0,"y":0,"w":3,"h":2,"s":{"style":"big","date":true}},
  {"id":"dk2","t":"weather_now","x":3,"y":0,"w":3,"h":2,"s":{}},
  {"id":"dk3","t":"status","x":6,"y":0,"w":4,"h":1,"s":{}},
  {"id":"dk4","t":"stocks","x":6,"y":1,"w":4,"h":3,"s":{"symbols":"AAPL,MSFT,NVDA","spark":true}},
  {"id":"dk5","t":"agenda","x":0,"y":2,"w":6,"h":4,"s":{"range":"days","days":3,"max":6}},
  {"id":"dk6","t":"stock_chart","x":6,"y":4,"w":4,"h":2,"s":{"symbol":"AAPL","range":"1M"}}]},
 {"id":"ready","name":"Get Ready","o":"L","theme":"light","w":[
  {"id":"gr1","t":"outfit","x":0,"y":0,"w":6,"h":6,"s":{}},
  {"id":"gr2","t":"hourly","x":6,"y":0,"w":4,"h":2,"s":{"hours":12,"show":"rain"}},
  {"id":"gr3","t":"next_event","x":6,"y":2,"w":4,"h":2,"s":{}},
  {"id":"gr4","t":"clock","x":6,"y":4,"w":4,"h":2,"s":{"style":"big","date":true}}]},
 {"id":"glance_p","name":"Glance","o":"P","theme":"light","w":[
  {"id":"gp1","t":"clock","x":0,"y":0,"w":6,"h":3,"s":{"style":"big","date":false}},
  {"id":"gp2","t":"date","x":0,"y":3,"w":6,"h":1,"s":{}},
  {"id":"gp3","t":"weather_now","x":0,"y":4,"w":6,"h":5,"s":{}},
  {"id":"gp4","t":"status","x":0,"y":9,"w":6,"h":1,"s":{}}]},
 {"id":"glance_l","name":"Glance","o":"L","theme":"light","w":[
  {"id":"gl1","t":"clock","x":0,"y":0,"w":5,"h":4,"s":{"style":"big","date":false}},
  {"id":"gl2","t":"date","x":0,"y":4,"w":5,"h":1,"s":{}},
  {"id":"gl3","t":"weather_now","x":5,"y":0,"w":5,"h":5,"s":{}},
  {"id":"gl4","t":"status","x":0,"y":5,"w":10,"h":1,"s":{}}]}],
"moments":[
 {"id":"m1","name":"Good Morning","on":true,"what":"screen:morning","poses":["PS"],"dp":["morning"],"days":127,"cond":[]},
 {"id":"m2","name":"Wind Down","on":true,"what":"screen:night","poses":["PS"],"dp":["night"],"days":127,"cond":[]},
 {"id":"m3","name":"Bedside","on":true,"what":"screen:bedside","poses":["PS"],"dp":[],"days":127,"cond":[]},
 {"id":"m4","name":"At the Desk","on":true,"what":"screen:desk","poses":["LS"],"dp":[],"days":127,"cond":[]},
 {"id":"m5","name":"Kitchen","on":true,"what":"app:chef","poses":["PV"],"dp":[],"days":127,"cond":[]},
 {"id":"m6","name":"Get Ready","on":true,"what":"screen:ready","poses":["LV"],"dp":[],"days":127,"cond":[]}],
"autos":[
 {"id":"a1","name":"Low Battery","on":true,"trig":{"t":"battery_below","v":15},"cond":[],"cool":360,"act":[
  {"t":"notify","title":"Moments battery low","msg":"Battery at {{battery}}%. Please charge me.","pri":4},
  {"t":"show","what":"sys:lowbat","min":10}]},
 {"id":"a2","name":"Fresh Air","on":false,"trig":{"t":"hum_above","v":70},"cond":[],"cool":360,"act":[
  {"t":"notify","title":"Time to ventilate","msg":"Humidity is {{humidity}}%. Open a window.","pri":3}]},
 {"id":"a3","name":"Too Cold","on":false,"trig":{"t":"temp_below","v":16},"cond":[],"cool":180,"act":[
  {"t":"notify","title":"Too cold","msg":"It is {{temp}} at home.","pri":3}]},
 {"id":"a4","name":"Too Hot","on":false,"trig":{"t":"temp_above","v":28},"cond":[],"cool":180,"act":[
  {"t":"notify","title":"Too hot","msg":"It is {{temp}} at home.","pri":3}]},
 {"id":"a5","name":"Wake Up","on":false,"trig":{"t":"wake_time"},"cond":[{"t":"days","v":31},{"t":"pose","v":["PS"]}],"cool":60,"act":[
  {"t":"sound","preset":"wake"}]}],
"apps":{
 "chef":{"diet":"","allergies":"","servings":2,"maxTime":30,"cuisines":"","windows":{"breakfast":"05:00","lunch":"11:00","dinner":"15:00","snack":"22:00"}},
 "timer":{"presets":[5,10,25,45],"brk":5,"sound":"double"},
 "gallery":{"interval":60,"shuffle":false,"gray":true},
 "guardian":{"on":false,"poses":["PV","LV"],"grace":20,"siren":true,"sirenSec":10}},
"content":{
 "pantry":["eggs","bread","milk","butter","cheese","tomatoes","onions","garlic","rice","pasta","olive oil","spinach","yogurt","oats","bananas","chicken breast","potatoes","lemons"],
 "wardrobe":[
  {"id":"t1","name":"White T-shirt","cat":"top","warm":1,"rain":false,"form":1,"color":"white"},
  {"id":"t2","name":"Blue oxford shirt","cat":"top","warm":2,"rain":false,"form":3,"color":"blue"},
  {"id":"t3","name":"Grey sweater","cat":"top","warm":3,"rain":false,"form":2,"color":"grey"},
  {"id":"b1","name":"Jeans","cat":"bottom","warm":2,"rain":false,"form":1,"color":"blue"},
  {"id":"b2","name":"Chinos","cat":"bottom","warm":2,"rain":false,"form":3,"color":"beige"},
  {"id":"b3","name":"Shorts","cat":"bottom","warm":1,"rain":false,"form":1,"color":"khaki"},
  {"id":"o1","name":"Rain jacket","cat":"outer","warm":2,"rain":true,"form":1,"color":"navy"},
  {"id":"o2","name":"Wool coat","cat":"outer","warm":5,"rain":false,"form":4,"color":"charcoal"},
  {"id":"o3","name":"Light hoodie","cat":"outer","warm":2,"rain":false,"form":1,"color":"grey"},
  {"id":"s1","name":"Sneakers","cat":"shoes","warm":2,"rain":false,"form":1,"color":"white"},
  {"id":"s2","name":"Leather boots","cat":"shoes","warm":3,"rain":true,"form":3,"color":"brown"},
  {"id":"x1","name":"Umbrella","cat":"accessory","warm":0,"rain":true,"form":1,"color":"black"},
  {"id":"x2","name":"Sunglasses","cat":"accessory","warm":0,"rain":false,"form":1,"color":"black"},
  {"id":"x3","name":"Scarf","cat":"accessory","warm":4,"rain":false,"form":2,"color":"red"}],
 "notes":[{"id":"n1","name":"Welcome","text":"Welcome to Moments. Configure me at http://moments.local"}],
 "quotes":[],
 "builtinQuotes":true},
"sound":{"vol":70,"quiet":false,"quietStart":"22:00","quietEnd":"07:00"},
"sys":{"profile":"balanced","chargeLimit":false,"partialAfterSleep":true,"fullEvery":10,"grayswap":false,"showName":true,
 "vertMax":20,"standMin":30,"standMax":60,"still":10.0,"motionThs":3,"tempOff":0,"humOff":0,"insecureTls":false},
"imu":{"xa":0,"xs":1,"ya":1,"ys":-1,"cal":false},
"touch":{"swap":true,"ix":true,"iy":false}
})JSON";

static const char DEFAULT_LISTS[] PROGMEM =
    R"JSON([{"name":"Shopping","items":[{"t":"Milk","d":false},{"t":"Coffee","d":false}]},{"name":"To-do","items":[{"t":"Water the plants","d":false}]}])JSON";

static const char* const SECRET_KEYS[] = {"wifi0p", "wifi1p", "wifi2p", "ai_key", "stk_key", "ntfy_topic", "ntfy_tok",
                                          "ics0",   "ics1",   "ics2",   "discord", "tg_tok", "webhook"};
static const int SECRET_COUNT = sizeof(SECRET_KEYS) / sizeof(SECRET_KEYS[0]);

Preferences prefs;

bool secretGet(const char* key, String& out) {
  prefs.begin("sec", true);
  out = prefs.getString(key, "");
  prefs.end();
  return out.length() > 0;
}
static bool secretHas(const char* key) {
  String s;
  return secretGet(key, s);
}
static void secretSet(const char* key, const String& v) {
  prefs.begin("sec", false);
  if (v.length()) prefs.putString(key, v);
  else prefs.remove(key);
  prefs.end();
}
static bool isSecretKey(const char* k) {
  for (int i = 0; i < SECRET_COUNT; i++)
    if (!strcmp(k, SECRET_KEYS[i])) return true;
  return false;
}

static String devGet(const char* key, const char* def = "") {
  prefs.begin("dev", true);
  String s = prefs.getString(key, def);
  prefs.end();
  return s;
}
static void devSet(const char* key, const String& v) {
  prefs.begin("dev", false);
  prefs.putString(key, v);
  prefs.end();
}

static void ensureNamespaces() {
  prefs.begin("sec", false);
  prefs.end();
  prefs.begin("dev", false);
  prefs.end();
}

// Adds keys missing in dst from src (objects only, recursive). Arrays are never merged.
static void mergeDefaults(JsonObject dst, JsonObjectConst src) {
  for (JsonPairConst kv : src) {
    if (dst[kv.key()].isNull()) {
      dst[kv.key()] = kv.value();
    } else if (dst[kv.key()].is<JsonObject>() && kv.value().is<JsonObjectConst>()) {
      mergeDefaults(dst[kv.key()].as<JsonObject>(), kv.value().as<JsonObjectConst>());
    }
  }
}

bool saveConfig() {
  if (!gFsOk) return false;
  File f = LittleFS.open("/config.tmp", "w");
  if (!f) return false;
  size_t n = serializeJson(cfg, f);
  f.close();
  if (n == 0) return false;
  LittleFS.remove(CFG_PATH);
  return LittleFS.rename("/config.tmp", CFG_PATH);
}

static void loadConfig() {
  bool ok = false;
  if (gFsOk) {
    File f = LittleFS.open(CFG_PATH, "r");
    if (f) {
      DeserializationError e = deserializeJson(cfg, f);
      f.close();
      ok = !e && cfg["v"].is<int>();
      if (!ok) LOGF("[cfg] parse error: %s\n", e.c_str());
    }
  }
  JsonDocument def(&g_psAlloc);
  deserializeJson(def, DEFAULT_CONFIG);
  if (!ok) {
    cfg.set(def);
    saveConfig();
    LOGF("[cfg] defaults written\n");
  } else {
    mergeDefaults(cfg.as<JsonObject>(), def.as<JsonObjectConst>());
  }
}

static void loadLists() {
  bool ok = false;
  if (gFsOk) {
    File f = LittleFS.open(LISTS_PATH, "r");
    if (f) {
      ok = !deserializeJson(listsDoc, f) && listsDoc.is<JsonArray>();
      f.close();
    }
  }
  if (!ok) deserializeJson(listsDoc, DEFAULT_LISTS);
}
static void saveLists() {
  if (!gFsOk) return;
  File f = LittleFS.open(LISTS_PATH, "w");
  if (!f) return;
  serializeJson(listsDoc, f);
  f.close();
}

static void applyHwConfig() {
  JsonObjectConst t = cfg["touch"];
  gTouchCal.swap = t["swap"] | true;
  gTouchCal.ix = t["ix"] | true;
  gTouchCal.iy = t["iy"] | false;
  const char* tz = cfg["loc"]["tz"] | "UTC0";
  setenv("TZ", tz, 1);
  tzset();
}

// Lookups ------------------------------------------------------------------
static JsonObject findById(JsonArray arr, const char* id) {
  if (!id) return JsonObject();
  for (JsonObject o : arr)
    if (!strcmp(o["id"] | "", id)) return o;
  return JsonObject();
}
static JsonObject findScreen(const char* id) { return findById(cfg["screens"].as<JsonArray>(), id); }

static int sysInt(const char* k, int def) { return cfg["sys"][k] | def; }
static float sysFloat(const char* k, float def) { return cfg["sys"][k] | def; }
static bool sysBool(const char* k, bool def) { return cfg["sys"][k] | def; }
static bool unitsF() { return !strcmp(cfg["profile"]["units"] | "C", "F"); }
static bool use24h() { return cfg["profile"]["h24"] | true; }
static bool hasLocation() { return cfg["loc"]["lat"].is<float>() && cfg["loc"]["lon"].is<float>(); }

static void factoryReset() {
  LittleFS.format();
  prefs.begin("sec", false);
  prefs.clear();
  prefs.end();
  prefs.begin("dev", false);
  prefs.clear();
  prefs.end();
  memset(&R, 0, sizeof(R));
  delay(200);
  ESP.restart();
}
