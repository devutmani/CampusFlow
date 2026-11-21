// ============================================================================
//  server.cpp  -  CampusFlow: WEB PART ONLY (sockets, HTTP, JSON, URLs)
//
//  main.cpp includes this file, so compile ONLY src/main.cpp (do not compile this alone).
//  There is NO data-structure logic here. Every api...() function does 3 steps:
//     1. read the values from the URL
//     2. call a function from main.cpp   (sort, search, stack, linked list, ...)
//     3. write the answer as JSON for the web page
// ============================================================================
#include <chrono>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
  #include <winsock2.h>
  typedef SOCKET Sock;
  #define CLOSE closesocket
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  typedef int Sock;
  #define CLOSE close
#endif
// ============================================================ JSON output (written by hand)
string js(const string &s) {                      // text -> "text" with escapes
    string o = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c < 32) o += ' ';
        else o += c;
    }
    return o + "\"";
}
string jc(const Cls &c) {                         // one class -> JSON object
    return "{\"id\":" + to_string(c.id) + ",\"course\":" + js(c.course) + ",\"code\":" + js(c.code) +
           ",\"teacher\":" + js(c.teacher) + ",\"section\":" + js(c.section) + ",\"room\":" + js(c.room) +
           ",\"day\":" + js(c.day) + ",\"start\":" + to_string(c.start) + ",\"end\":" + to_string(c.end) + "}";
}
string jlist(const vector<Cls> &v, size_t limit = 100000) {
    string o = "[";
    for (size_t i = 0; i < v.size() && i < limit; i++) o += (i ? "," : "") + jc(v[i]);
    return o + "]";
}
string reply(bool ok, const string &msg) { return "{\"ok\":" + string(ok ? "true" : "false") + ",\"msg\":" + js(msg) + "}"; }

// ============================================================ API (one function per URL)
typedef map<string, string> Params;
string get(const Params &p, const string &k, const string &def = "") { auto i = p.find(k); return i == p.end() ? def : i->second; }

// /api/list  -> linear search + sorting (bubble / merge / quick)
string apiList(const Params &p) {
    vector<Cls> hits = linearSearch(timetable, get(p, "field", "course"), get(p, "q"));
    string key = get(p, "key", "time");
    int sortKey = key == "course" ? 2 : key == "teacher" ? 3 : key == "room" ? 4 : 1;
    auto t0 = chrono::steady_clock::now();
    long long compares = sortBy(hits, get(p, "algo", "merge"), sortKey);
    double ms = chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count();
    int limit = atoi(get(p, "limit", "100").c_str());
    return "{\"total\":" + to_string(timetable.size()) + ",\"hits\":" + to_string(hits.size()) +
           ",\"compares\":" + to_string(compares) + ",\"ms\":" + to_string(ms) + ",\"undo\":" +
           to_string(undoStack.size()) + ",\"rows\":" + jlist(hits, limit) + "}";
}

// /api/courses -> list of different course names
string apiCourses(const Params &) {
    vector<string> names;
    for (const Cls &c : timetable) names.push_back(c.course);
    sort(names.begin(), names.end());
    names.erase(unique(names.begin(), names.end()), names.end());
    string o = "[";
    for (const string &n : names) o += (o.size() > 1 ? "," : "") + js(n);
    return o + "]";
}

// /api/course -> binary search
string apiCourse(const Params &p) {
    int steps = 0;
    string name;
    if (!findCourse(timetable, get(p, "name"), steps, name)) return "{\"found\":false,\"steps\":" + to_string(steps) + "}";
    return "{\"found\":true,\"steps\":" + to_string(steps) + ",\"name\":" + js(name) + "}";
}

// /api/add -> checks the typed values, then core addClass()
string apiAdd(const Params &p) {
    Cls c;
    c.course = trim(get(p, "course")); c.code = trim(get(p, "code")); c.teacher = trim(get(p, "teacher"));
    c.section = trim(get(p, "section")); c.room = trim(get(p, "room"));
    int d = dayNo(get(p, "day"));
    bool ok = parseTime(get(p, "start"), c.start) && parseTime(get(p, "end"), c.end);
    for (const string *s : {&c.course, &c.teacher, &c.section, &c.room})
        if (s->empty() || s->find(',') != string::npos) ok = false;
    if (c.code.empty() || c.code.find(',') != string::npos) c.code = "-";
    if (!ok || d < 0 || c.start >= c.end) return reply(false, "Invalid input: fill all fields (no commas) with a valid day and times.");
    c.day = DAYS[d]; c.dn = d;
    string msg;
    bool done = addClass(c, msg);                 // <-- data structure logic
    return reply(done, msg);
}

string apiRemove(const Params &p) { string msg; bool ok = removeClass(atoi(get(p, "id").c_str()), msg); return reply(ok, msg); }
string apiUndo(const Params &)    { string msg; bool ok = undoLast(msg); return reply(ok, msg); }
string apiSave(const Params &) {
    return saveCSV(TIMETABLE_FILE, timetable) ? reply(true, "Saved to " + TIMETABLE_FILE) : reply(false, "Could not save the file.");
}

// /api/clashes
string apiClashes(const Params &p) {
    vector<pair<Cls, Cls> > pairs;
    int count = findClashes(get(p, "type", "room"), pairs, 40);
    string o = "[";
    for (size_t i = 0; i < pairs.size(); i++)
        o += (i ? "," : "") + string("{\"a\":") + jc(pairs[i].first) + ",\"b\":" + jc(pairs[i].second) + "}";
    return "{\"count\":" + to_string(count) + ",\"pairs\":" + o + "]}";
}

// /api/free -> free classroom finder
string apiFree(const Params &p) {
    Cls slot;
    int d = dayNo(get(p, "day"));
    if (d < 0 || !parseTime(get(p, "start"), slot.start) || !parseTime(get(p, "end"), slot.end) || slot.start >= slot.end)
        return reply(false, "Invalid day or time.");
    slot.day = DAYS[d];
    int total = 0;
    vector<string> freeList = freeRooms(slot, total);
    string o = "[";
    for (size_t i = 0; i < freeList.size(); i++) o += (i ? "," : "") + js(freeList[i]);
    return "{\"ok\":true,\"total\":" + to_string(total) + ",\"free\":" + o + "]}";
}

// /api/plan -> backtracking course planner
string apiPlan(const Params &p) {
    vector<string> names;
    stringstream ss(get(p, "courses"));
    string name;
    while (getline(ss, name, '|')) names.push_back(name);
    PlanResult r = planCourses(names);             // <-- backtracking logic
    if (!r.error.empty()) return reply(false, r.error);
    string plans;
    for (size_t k = 0; k < r.firstPlans.size(); k++) {
        string one = "[";
        for (size_t i = 0; i < r.firstPlans[k].size(); i++) {
            const Option &o = r.firstPlans[k][i];
            one += (i ? "," : "") + string("{\"course\":") + js(o.course) + ",\"section\":" + js(o.section) +
                   ",\"rows\":" + jlist(o.rows) + "}";
        }
        plans += (k ? "," : "") + one + "]";
    }
    return "{\"ok\":true,\"count\":" + to_string(r.count) + ",\"backtracks\":" + to_string(r.backtracks) +
           ",\"plans\":[" + plans + "]}";
}

// ---- my timetable (singly linked list). Saved to my_timetable.csv after every change.
void saveMine() { saveCSV(MY_FILE, mine.toVector()); }

string apiMine(const Params &) { return "{\"rows\":" + jlist(mine.toVector()) + "}"; }

string apiMineAdd(const Params &p) {              // ids=1,2,3
    stringstream ss(get(p, "ids"));
    string tok, lastBad, why;
    int added = 0, skipped = 0;
    while (getline(ss, tok, ',')) {
        int r = mineAddById(atoi(tok.c_str()), why);   // <-- linked list logic
        if (r == 1) added++;
        else if (r == 0) { skipped++; lastBad = why; }
    }
    saveMine();
    string msg = to_string(added) + " class(es) added";
    if (skipped) msg += ", " + to_string(skipped) + " skipped (" + lastBad + ")";
    return reply(added > 0, added > 0 ? msg : lastBad);
}
string apiMineRemove(const Params &p) {
    bool ok = mine.removeAt(atoi(get(p, "pos", "-1").c_str()));
    saveMine();
    return reply(ok, ok ? "Removed." : "Not found.");
}
string apiMineClear(const Params &) { mine.clear(); saveMine(); return reply(true, "Timetable cleared."); }

// URL -> function
string route(const string &path, const Params &p, bool &found) {
    typedef string (*Handler)(const Params &);
    static const map<string, Handler> routes = {
        {"/api/list", apiList}, {"/api/courses", apiCourses}, {"/api/course", apiCourse},
        {"/api/add", apiAdd}, {"/api/remove", apiRemove}, {"/api/undo", apiUndo}, {"/api/save", apiSave},
        {"/api/clashes", apiClashes}, {"/api/free", apiFree}, {"/api/plan", apiPlan},
        {"/api/mine", apiMine}, {"/api/mine/add", apiMineAdd}, {"/api/mine/remove", apiMineRemove},
        {"/api/mine/clear", apiMineClear}};
    auto it = routes.find(path);
    found = it != routes.end();
    return found ? it->second(p) : "";
}

// ============================================================ web server (sockets)
string urlDecode(const string &s) {
    string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '+') o += ' ';
        else if (s[i] == '%' && i + 2 < s.size()) { o += (char)strtol(s.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; }
        else o += s[i];
    }
    return o;
}
Params parseQuery(const string &q) {              // "a=1&b=2" -> {a:1, b:2}
    Params p;
    stringstream ss(q);
    string pair;
    while (getline(ss, pair, '&')) {
        size_t eq = pair.find('=');
        if (eq != string::npos) p[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
    }
    return p;
}
void sendAll(Sock s, const string &status, const string &type, const string &body) {
    string r = "HTTP/1.1 " + status + "\r\nContent-Type: " + type + "\r\nContent-Length: " + to_string(body.size()) +
               "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n" + body;
    size_t sent = 0;
    while (sent < r.size()) {
        int n = (int)send(s, r.data() + sent, (int)(r.size() - sent), 0);
        if (n <= 0) break;
        sent += n;
    }
}
string readFile(const string &name) { ifstream f(name, ios::binary); stringstream ss; ss << f.rdbuf(); return ss.str(); }

void handle(Sock client) {
    char buf[8192];
    int n = (int)recv(client, buf, sizeof buf - 1, 0);
    if (n <= 0) return;
    buf[n] = 0;
    string method, target;
    stringstream(string(buf)) >> method >> target;          // first line: GET /path?query HTTP/1.1
    size_t qm = target.find('?');
    string path = target.substr(0, qm);
    Params params = parseQuery(qm == string::npos ? "" : target.substr(qm + 1));

    if (path == "/" || path == "/index.html") {
        string page = readFile(PAGE_FILE);
        if (page.empty()) sendAll(client, "404 Not Found", "text/plain", "web/campusflow.html not found. Run the program from the CampusFlow folder.");
        else sendAll(client, "200 OK", "text/html; charset=utf-8", page);
        return;
    }
    bool found;
    string body = route(path, params, found);
    if (found) sendAll(client, "200 OK", "application/json; charset=utf-8", body);
    else sendAll(client, "404 Not Found", "text/plain", "Not found");
}

int runServer() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    Sock server = socket(AF_INET, SOCK_STREAM, 0);
#ifndef _WIN32
    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof yes);
#endif
    sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);          // only this computer can connect
    int port = 8080;
    for (; port < 8090; port++) {                           // if 8080 is busy, try the next one
        addr.sin_port = htons(port);
        if (bind(server, (sockaddr *)&addr, sizeof addr) == 0) break;
    }
    if (port == 8090 || listen(server, 10) != 0) { cout << "Error: could not start the server.\n"; return 1; }

    string url = "http://localhost:" + to_string(port);
    cout << "CampusFlow is running at " << url << "\nPress Ctrl+C to stop.\n";
#ifdef _WIN32
    system(("start " + url).c_str());
#elif defined(__APPLE__)
    system(("open " + url).c_str());
#endif

    while (true) {                                          // one request at a time is enough here
        Sock client = accept(server, nullptr, nullptr);
        if (client == (Sock)-1) continue;
        handle(client);
        CLOSE(client);
    }
}
