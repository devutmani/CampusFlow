// CampusFlow - timetable manager. ALL logic is in C++; the browser is only the screen.
// Run the program, then open http://localhost:8080  (it opens by itself on Windows/Mac).
//
// Build (Linux / Mac):  g++ -std=c++17 -O2 main.cpp -o campusflow
// Build (Windows MinGW): g++ -std=c++17 -O2 main.cpp -o campusflow -lws2_32
// Needs in the same folder: timetable.csv and campusflow.html
//
// DSA used: vector, singly linked list, stack (undo), linear search, binary search,
//           bubble/merge/quick sort, recursion, backtracking.
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stack>
#include <set>
#include <map>
#include <chrono>
#include <algorithm>
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
using namespace std;

// ---------------------------------------------------------------- data
struct Cls {
    int id = 0;                                  // never changes, even after sorting/removing
    string course, code, teacher, section, room, day;
    int dn = 0;                                  // day number 0..5 (fast to compare when sorting)
    int start = 0, end = 0;                      // minutes from midnight
};
const string DAYS[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
vector<Cls> timetable;
int nextId = 1;

// ------------------------------------------------------------- helpers
string lower(string s) { for (char &c : s) c = tolower((unsigned char)c); return s; }

string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    return a == string::npos ? "" : s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
int dayNo(string d) {                            // "monday" / "MON" -> 0 ; bad -> -1
    d = lower(trim(d));
    for (int i = 0; i < 6; i++) if (d.size() >= 3 && d.substr(0, 3) == lower(DAYS[i])) return i;
    return -1;
}
string hhmm(int m) { char b[16]; snprintf(b, sizeof b, "%02d:%02d", m / 60, m % 60); return b; }

bool parseTime(const string &t, int &out) {     // "09:30" -> 570
    int h, m; char c;
    stringstream ss(t);
    if (!(ss >> h >> c >> m) || c != ':' || h < 0 || h > 23 || m < 0 || m > 59) return false;
    out = h * 60 + m;
    return true;
}
bool overlap(const Cls &a, const Cls &b) { return a.day == b.day && a.start < b.end && b.start < a.end; }

// Two sections sharing course+teacher+room+time are one joint class, not a clash.
bool joint(const Cls &a, const Cls &b) {
    return a.course == b.course && a.teacher == b.teacher && a.room == b.room &&
           a.day == b.day && a.start == b.start && a.end == b.end;
}

// ---- JSON output (written by hand, no library needed)
string js(const string &s) {
    string o = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c < 32) o += ' ';
        else o += c;
    }
    return o + "\"";
}
string jc(const Cls &c) {
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
string describe(const Cls &c) { return c.course + " (" + c.section + ", " + c.day + " " + hhmm(c.start) + ")"; }

// ------------------------------------------------------------ CSV file
bool loadCSV(const string &file, vector<Cls> &out) {
    ifstream in(file);
    if (!in) return false;
    string line;
    getline(in, line);                           // skip header
    while (getline(in, line)) {
        if (trim(line).empty()) continue;
        vector<string> f;
        stringstream ss(line);
        string x;
        while (getline(ss, x, ',')) f.push_back(trim(x));
        if (f.size() < 8) continue;
        Cls c;
        c.course = f[0]; c.code = f[1]; c.teacher = f[2]; c.section = f[3]; c.room = f[4];
        int d = dayNo(f[5]);
        if (d < 0 || !parseTime(f[6], c.start) || !parseTime(f[7], c.end) || c.start >= c.end) continue;
        c.day = DAYS[d]; c.dn = d;
        out.push_back(c);
    }
    return true;
}
bool saveCSV(const string &file, const vector<Cls> &v) {
    ofstream out(file);
    if (!out) return false;
    out << "Course,Code,Teacher,Section,Room,Day,Start,End\n";
    for (const Cls &c : v)
        out << c.course << ',' << c.code << ',' << c.teacher << ',' << c.section << ',' << c.room << ','
            << c.day << ',' << hhmm(c.start) << ',' << hhmm(c.end) << '\n';
    return true;
}

// -------------------------------------------------------------- sorting
// key: 1 = day & time, 2 = course, 3 = teacher, 4 = room
long long compares = 0;
int sortKey = 1;

bool before(const Cls &a, const Cls &b) {
    compares++;
    if (sortKey == 2 && a.course != b.course) return lower(a.course) < lower(b.course);
    if (sortKey == 3 && a.teacher != b.teacher) return lower(a.teacher) < lower(b.teacher);
    if (sortKey == 4 && a.room != b.room) return a.room < b.room;
    if (a.dn != b.dn) return a.dn < b.dn;
    return a.start < b.start;
}
void bubbleSort(vector<Cls> &v) {
    for (size_t i = 0; i + 1 < v.size(); i++) {
        bool swapped = false;
        for (size_t j = 0; j + 1 < v.size() - i; j++)
            if (before(v[j + 1], v[j])) { swap(v[j], v[j + 1]); swapped = true; }
        if (!swapped) break;                     // already sorted
    }
}
void mergeSort(vector<Cls> &v, int l, int r) {   // sorts v[l..r)
    if (r - l < 2) return;
    int m = (l + r) / 2;
    mergeSort(v, l, m);
    mergeSort(v, m, r);
    vector<Cls> tmp;
    int i = l, j = m;
    while (i < m && j < r) tmp.push_back(before(v[j], v[i]) ? v[j++] : v[i++]);
    while (i < m) tmp.push_back(v[i++]);
    while (j < r) tmp.push_back(v[j++]);
    copy(tmp.begin(), tmp.end(), v.begin() + l);
}
void quickSort(vector<Cls> &v, int lo, int hi) { // sorts v[lo..hi]
    if (lo >= hi) return;
    swap(v[(lo + hi) / 2], v[hi]);               // middle pivot avoids the slow case
    int p = lo;
    for (int i = lo; i < hi; i++) if (before(v[i], v[hi])) swap(v[i], v[p++]);
    swap(v[p], v[hi]);
    quickSort(v, lo, p - 1);
    quickSort(v, p + 1, hi);
}
void sortBy(vector<Cls> &v, const string &algo) {
    if (algo == "bubble") bubbleSort(v);
    else if (algo == "quick") quickSort(v, 0, (int)v.size() - 1);
    else mergeSort(v, 0, (int)v.size());
}

// --------------------------------------------------- stack: undo changes
struct Action { bool wasAdd; Cls rec; };
stack<Action> undoStack;

// ------------------------------------- singly linked list: my timetable
struct Node { Cls data; Node *next; };
class MyTimetable {
    Node *head = nullptr;
public:
    ~MyTimetable() { clear(); }
    void clear() { while (head) { Node *n = head; head = head->next; delete n; } }
    void add(const Cls &c) {                     // add at the end
        Node *n = new Node{c, nullptr};
        if (!head) { head = n; return; }
        Node *p = head;
        while (p->next) p = p->next;
        p->next = n;
    }
    bool removeAt(int pos) {                     // pos starts at 0
        if (pos < 0 || !head) return false;
        if (pos == 0) { Node *n = head; head = head->next; delete n; return true; }
        Node *prev = head;
        for (int i = 1; i < pos && prev->next; i++) prev = prev->next;
        if (!prev->next) return false;
        Node *n = prev->next;
        prev->next = n->next;
        delete n;
        return true;
    }
    bool has(int id) const { for (Node *p = head; p; p = p->next) if (p->data.id == id) return true; return false; }
    const Cls *clashWith(const Cls &c) const {
        for (Node *p = head; p; p = p->next) if (overlap(p->data, c)) return &p->data;
        return nullptr;
    }
    vector<Cls> toVector() const { vector<Cls> v; for (Node *p = head; p; p = p->next) v.push_back(p->data); return v; }
} mine;

// ------------------------------------------------------------- searching
const string &fieldOf(const Cls &c, const string &f) {
    return f == "section" ? c.section : f == "teacher" ? c.teacher : f == "room" ? c.room : f == "day" ? c.day : c.course;
}
vector<Cls> linearSearch(const vector<Cls> &v, const string &field, const string &text) {
    vector<Cls> hits;
    string t = lower(text);
    for (const Cls &c : v) if (lower(fieldOf(c, field)).find(t) != string::npos) hits.push_back(c);
    return hits;
}

// ------------------------------------------------------ clashes & rooms
const Cls *findConflict(const Cls &c) {          // does c clash with the timetable?
    for (const Cls &e : timetable)
        if (overlap(e, c) && !joint(e, c) && (e.room == c.room || e.teacher == c.teacher || e.section == c.section))
            return &e;
    return nullptr;
}

// ------------------------------------------- planner (backtracking)
// One section must be picked for every course so that no two picked sections overlap.
struct Option { string course, section; vector<Cls> rows; };
vector<vector<Option>> groups;
vector<Option> picked;
long long plans = 0, backtracks = 0;
string planJson;

bool optionsClash(const Option &a, const Option &b) {
    for (const Cls &x : a.rows) for (const Cls &y : b.rows) if (overlap(x, y)) return true;
    return false;
}
void plan(size_t level) {                        // recursion
    if (plans >= 10000) return;
    if (level == groups.size()) {                // every course has a section
        if (++plans <= 3) {
            string p = "[";
            for (size_t i = 0; i < picked.size(); i++)
                p += (i ? "," : "") + string("{\"course\":") + js(picked[i].course) + ",\"section\":" +
                     js(picked[i].section) + ",\"rows\":" + jlist(picked[i].rows) + "}";
            planJson += (planJson.empty() ? "" : ",") + p + "]";
        }
        return;
    }
    for (const Option &opt : groups[level]) {
        bool bad = false;
        for (const Option &p : picked) if (optionsClash(p, opt)) { bad = true; break; }
        if (bad) { backtracks++; continue; }     // clash: skip this section
        picked.push_back(opt);                   // choose
        plan(level + 1);                         // go deeper
        picked.pop_back();                       // undo = backtrack
    }
}

// =================================================== API (one function per URL)
typedef map<string, string> Params;
string get(const Params &p, const string &k, const string &def = "") { auto i = p.find(k); return i == p.end() ? def : i->second; }

string apiList(const Params &p) {
    vector<Cls> hits = linearSearch(timetable, get(p, "field", "course"), get(p, "q"));
    string key = get(p, "key", "time");
    sortKey = key == "course" ? 2 : key == "teacher" ? 3 : key == "room" ? 4 : 1;
    compares = 0;
    auto t0 = chrono::steady_clock::now();
    sortBy(hits, get(p, "algo", "merge"));
    double ms = chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count();
    int limit = atoi(get(p, "limit", "100").c_str());
    return "{\"total\":" + to_string(timetable.size()) + ",\"hits\":" + to_string(hits.size()) +
           ",\"compares\":" + to_string(compares) + ",\"ms\":" + to_string(ms) + ",\"undo\":" +
           to_string(undoStack.size()) + ",\"rows\":" + jlist(hits, limit) + "}";
}

string apiCourses(const Params &) {              // distinct course names, sorted A-Z
    set<string> names;
    for (const Cls &c : timetable) names.insert(c.course);
    string o = "[";
    for (const string &n : names) o += (o.size() > 1 ? "," : "") + js(n);
    return o + "]";
}

string apiCourse(const Params &p) {              // binary search by exact course name
    vector<Cls> s = timetable;
    sortKey = 2;
    mergeSort(s, 0, (int)s.size());
    string want = lower(trim(get(p, "name")));
    int lo = 0, hi = (int)s.size() - 1, steps = 0, found = -1;
    while (lo <= hi) {
        steps++;
        int mid = (lo + hi) / 2;
        string name = lower(s[mid].course);
        if (name == want) { found = mid; break; }
        if (name < want) lo = mid + 1; else hi = mid - 1;
    }
    if (found < 0) return "{\"found\":false,\"steps\":" + to_string(steps) + "}";
    return "{\"found\":true,\"steps\":" + to_string(steps) + ",\"name\":" + js(s[found].course) + "}";
}

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
    if (const Cls *bad = findConflict(c)) return reply(false, "Clash with " + describe(*bad));
    c.id = nextId++;
    timetable.push_back(c);
    undoStack.push({true, c});
    return reply(true, "Class added.");
}

string apiRemove(const Params &p) {
    int id = atoi(get(p, "id").c_str());
    for (size_t i = 0; i < timetable.size(); i++)
        if (timetable[i].id == id) {
            undoStack.push({false, timetable[i]});
            timetable.erase(timetable.begin() + i);
            return reply(true, "Class removed.");
        }
    return reply(false, "Class not found.");
}

string apiUndo(const Params &) {
    if (undoStack.empty()) return reply(false, "Nothing to undo.");
    Action a = undoStack.top();
    undoStack.pop();
    if (a.wasAdd) {                              // undo an add = remove it
        for (size_t i = 0; i < timetable.size(); i++)
            if (timetable[i].id == a.rec.id) { timetable.erase(timetable.begin() + i); break; }
        return reply(true, "Undone: added class removed.");
    }
    timetable.push_back(a.rec);                  // undo a remove = put it back
    return reply(true, "Undone: removed class restored.");
}

string apiSave(const Params &) {
    return saveCSV("timetable.csv", timetable) ? reply(true, "Saved to timetable.csv") : reply(false, "Could not save the file.");
}

string apiClashes(const Params &p) {
    string type = get(p, "type", "room");
    int count = 0;
    string pairs = "[";
    for (size_t i = 0; i < timetable.size(); i++)
        for (size_t j = i + 1; j < timetable.size(); j++) {
            const Cls &a = timetable[i], &b = timetable[j];
            if (!overlap(a, b) || joint(a, b) || fieldOf(a, type) != fieldOf(b, type)) continue;
            if (++count <= 40) pairs += (count > 1 ? "," : "") + string("{\"a\":") + jc(a) + ",\"b\":" + jc(b) + "}";
        }
    return "{\"count\":" + to_string(count) + ",\"pairs\":" + pairs + "]}";
}

string apiFree(const Params &p) {
    Cls slot;
    int d = dayNo(get(p, "day"));
    if (d < 0 || !parseTime(get(p, "start"), slot.start) || !parseTime(get(p, "end"), slot.end) || slot.start >= slot.end)
        return reply(false, "Invalid day or time.");
    slot.day = DAYS[d];
    set<string> all, busy;
    for (const Cls &c : timetable) { all.insert(c.room); if (overlap(c, slot)) busy.insert(c.room); }
    string o = "[";
    int n = 0;
    for (const string &r : all) if (!busy.count(r)) o += (n++ ? "," : "") + js(r);
    return "{\"ok\":true,\"total\":" + to_string(all.size()) + ",\"free\":" + o + "]}";
}

string apiPlan(const Params &p) {
    groups.clear(); picked.clear(); plans = backtracks = 0; planJson = "";
    stringstream ss(get(p, "courses"));
    string name;
    while (getline(ss, name, '|')) {
        map<string, Option> bySection;
        for (const Cls &c : timetable)
            if (lower(c.course) == lower(trim(name))) {
                Option &o = bySection[c.section];
                o.course = c.course; o.section = c.section; o.rows.push_back(c);
            }
        if (bySection.empty()) return reply(false, "Course not found: " + name);
        vector<Option> g;
        for (auto &kv : bySection) g.push_back(kv.second);
        groups.push_back(g);
    }
    if (groups.empty()) return reply(false, "Choose at least one course.");
    plan(0);
    return "{\"ok\":true,\"count\":" + to_string(plans) + ",\"backtracks\":" + to_string(backtracks) +
           ",\"plans\":[" + planJson + "]}";
}

// ---- my timetable (linked list). Saved to my_timetable.csv after every change.
void saveMine() { saveCSV("my_timetable.csv", mine.toVector()); }

string apiMine(const Params &) { return "{\"rows\":" + jlist(mine.toVector()) + "}"; }

string apiMineAdd(const Params &p) {             // ids=1,2,3
    stringstream ss(get(p, "ids"));
    string tok, lastBad;
    int added = 0, skipped = 0;
    while (getline(ss, tok, ',')) {
        int id = atoi(tok.c_str());
        for (const Cls &c : timetable) {
            if (c.id != id) continue;
            if (mine.has(id)) { skipped++; lastBad = "Already in your timetable"; }
            else if (const Cls *bad = mine.clashWith(c)) { skipped++; lastBad = "Clash with " + describe(*bad); }
            else { mine.add(c); added++; }
        }
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

// ================================================================ web server
string urlDecode(const string &s) {
    string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '+') o += ' ';
        else if (s[i] == '%' && i + 2 < s.size()) { o += (char)strtol(s.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; }
        else o += s[i];
    }
    return o;
}
Params parseQuery(const string &q) {
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
        string page = readFile("campusflow.html");
        if (page.empty()) sendAll(client, "404 Not Found", "text/plain", "campusflow.html not found next to the program.");
        else sendAll(client, "200 OK", "text/html; charset=utf-8", page);
        return;
    }
    bool found;
    string body = route(path, params, found);
    if (found) sendAll(client, "200 OK", "application/json; charset=utf-8", body);
    else sendAll(client, "404 Not Found", "text/plain", "Not found");
}

int main() {
    if (!loadCSV("timetable.csv", timetable)) {
        cout << "Error: timetable.csv not found. Put it in the same folder as the program.\n";
        return 1;
    }
    for (Cls &c : timetable) c.id = nextId++;
    vector<Cls> saved;
    if (loadCSV("my_timetable.csv", saved))                 // restore my timetable by matching classes
        for (const Cls &s : saved)
            for (const Cls &c : timetable)
                if (joint(c, s) && c.section == s.section) { mine.add(c); break; }
    cout << "Loaded " << timetable.size() << " classes, " << mine.toVector().size() << " in my timetable.\n";

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
