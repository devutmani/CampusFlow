#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
using namespace std;

// ---------------------------------------------------------------- one class
struct Cls {
    int id = 0;                                   // unique number, never changes
    string course, code, teacher, section, room, day;
    int dn = 0;                                   // day number 0..5 (Mon..Sat)
    int start = 0, end = 0;                       // minutes from midnight (09:00 = 540)
};
const string DAYS[6] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
vector<Cls> timetable;                            // the whole university timetable
int nextId = 1;

// ---------------------------------------------------------------- small helpers
string lower(string s) { for (char &c : s) c = tolower((unsigned char)c); return s; }

string trim(const string &s) {                    // remove spaces at both ends
    size_t a = s.find_first_not_of(" \t\r\n");
    return a == string::npos ? "" : s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
int dayNo(string d) {                             // "monday" / "MON" -> 0 ; wrong -> -1
    d = lower(trim(d));
    for (int i = 0; i < 6; i++) if (d.size() >= 3 && d.substr(0, 3) == lower(DAYS[i])) return i;
    return -1;
}
string hhmm(int m) { char b[16]; snprintf(b, sizeof b, "%02d:%02d", m / 60, m % 60); return b; }   // 540 -> "09:00"

bool parseTime(const string &t, int &out) {      // "09:30" -> 570
    int h, m; char c;
    stringstream ss(t);
    if (!(ss >> h >> c >> m) || c != ':' || h < 0 || h > 23 || m < 0 || m > 59) return false;
    out = h * 60 + m;
    return true;
}
bool overlap(const Cls &a, const Cls &b) { return a.day == b.day && a.start < b.end && b.start < a.end; }

// same course + teacher + room + time = one shared class (not a clash)
bool joint(const Cls &a, const Cls &b) {
    return a.course == b.course && a.teacher == b.teacher && a.room == b.room &&
           a.day == b.day && a.start == b.start && a.end == b.end;
}
const string &fieldOf(const Cls &c, const string &f) {
    return f == "section" ? c.section : f == "teacher" ? c.teacher : f == "room" ? c.room : f == "day" ? c.day : c.course;
}
string describe(const Cls &c) { return c.course + " (" + c.section + ", " + c.day + " " + hhmm(c.start) + ")"; }

// ---------------------------------------------------------------- CSV file
bool loadCSV(const string &file, vector<Cls> &out) {
    ifstream in(file);
    if (!in) return false;
    string line;
    getline(in, line);                            // skip the heading line
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

// ================================================================ 1. SORTING
long long compares = 0;                           // counts comparisons (to compare the algorithms)
int sortKey = 1;                                  // 1 = day+time, 2 = course, 3 = teacher, 4 = room

bool before(const Cls &a, const Cls &b) {         // true if a must come BEFORE b
    compares++;
    if (sortKey == 2 && a.course != b.course) return lower(a.course) < lower(b.course);
    if (sortKey == 3 && a.teacher != b.teacher) return lower(a.teacher) < lower(b.teacher);
    if (sortKey == 4 && a.room != b.room) return a.room < b.room;
    if (a.dn != b.dn) return a.dn < b.dn;
    return a.start < b.start;
}

void bubbleSort(vector<Cls> &v) {                 // swap neighbours that are in the wrong order
    for (size_t i = 0; i + 1 < v.size(); i++) {
        bool swapped = false;
        for (size_t j = 0; j + 1 < v.size() - i; j++)
            if (before(v[j + 1], v[j])) { swap(v[j], v[j + 1]); swapped = true; }
        if (!swapped) break;                      // no swap = already sorted
    }
}
void mergeSort(vector<Cls> &v, int l, int r) {    // sorts v[l..r): sort both halves, then merge
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
void quickSort(vector<Cls> &v, int lo, int hi) {  // sorts v[lo..hi]: pivot goes to its final place
    if (lo >= hi) return;
    swap(v[(lo + hi) / 2], v[hi]);                // middle item as pivot
    int p = lo;
    for (int i = lo; i < hi; i++) if (before(v[i], v[hi])) swap(v[i], v[p++]);
    swap(v[p], v[hi]);
    quickSort(v, lo, p - 1);
    quickSort(v, p + 1, hi);
}
long long sortBy(vector<Cls> &v, const string &algo, int key) {   // returns the number of comparisons
    sortKey = key;
    compares = 0;
    if (algo == "bubble") bubbleSort(v);
    else if (algo == "quick") quickSort(v, 0, (int)v.size() - 1);
    else mergeSort(v, 0, (int)v.size());
    return compares;
}

// ================================================================ 2. SEARCHING
vector<Cls> linearSearch(const vector<Cls> &v, const string &field, const string &text) {
    vector<Cls> hits;                             // look at every class one by one
    for (const Cls &c : v) if (lower(fieldOf(c, field)).find(lower(text)) != string::npos) hits.push_back(c);
    return hits;
}
bool findCourse(const vector<Cls> &all, const string &name, int &steps, string &foundName) {
    vector<Cls> s = all;                          // binary search needs sorted data: sort a copy
    sortBy(s, "merge", 2);
    string want = lower(trim(name));
    int lo = 0, hi = (int)s.size() - 1;
    steps = 0;
    while (lo <= hi) {                            // check the middle, throw away one half
        steps++;
        int mid = (lo + hi) / 2;
        string cur = lower(s[mid].course);
        if (cur == want) { foundName = s[mid].course; return true; }
        if (cur < want) lo = mid + 1; else hi = mid - 1;
    }
    return false;
}

// ================================================================ 3. STACK (undo), made with a linked list
struct Action { bool wasAdd; Cls rec; };          // one change we can undo
struct ActionNode { Action data; ActionNode *next; };

class UndoStack {
    ActionNode *top = nullptr;                    // the newest action
    int count = 0;
public:
    ~UndoStack() { Action a; while (pop(a)) {} }
    int size() const { return count; }
    void push(const Action &a) { top = new ActionNode{a, top}; count++; }   // new node becomes the top
    bool pop(Action &out) {
        if (!top) return false;                   // empty stack
        ActionNode *n = top;
        out = n->data;
        top = n->next;                            // the node below becomes the top
        delete n;
        count--;
        return true;
    }
} undoStack;

// does c clash with the timetable? (same room, teacher or section at the same time)
const Cls *findConflict(const Cls &c) {
    for (const Cls &e : timetable)
        if (overlap(e, c) && !joint(e, c) && (e.room == c.room || e.teacher == c.teacher || e.section == c.section))
            return &e;
    return nullptr;
}
bool addClass(Cls c, string &msg) {
    if (const Cls *bad = findConflict(c)) { msg = "Clash with " + describe(*bad); return false; }
    c.id = nextId++;
    timetable.push_back(c);
    undoStack.push({true, c});                    // remember: "added"
    msg = "Class added.";
    return true;
}
bool removeClass(int id, string &msg) {
    for (size_t i = 0; i < timetable.size(); i++)
        if (timetable[i].id == id) {
            undoStack.push({false, timetable[i]});    // remember: "removed"
            timetable.erase(timetable.begin() + i);
            msg = "Class removed.";
            return true;
        }
    msg = "Class not found.";
    return false;
}
bool undoLast(string &msg) {
    Action a;
    if (!undoStack.pop(a)) { msg = "Nothing to undo."; return false; }
    if (a.wasAdd) {                               // undo an add = remove it
        for (size_t i = 0; i < timetable.size(); i++)
            if (timetable[i].id == a.rec.id) { timetable.erase(timetable.begin() + i); break; }
        msg = "Undone: added class removed.";
    } else {                                      // undo a remove = put it back
        timetable.push_back(a.rec);
        msg = "Undone: removed class restored.";
    }
    return true;
}

// ================================================================ 4. SINGLY LINKED LIST (my timetable)
struct ClassNode { Cls data; ClassNode *next; };

class MyTimetable {
    ClassNode *head = nullptr;
public:
    ~MyTimetable() { clear(); }
    void clear() { while (head) { ClassNode *n = head; head = head->next; delete n; } }
    void add(const Cls &c) {                      // INSERT at the end
        ClassNode *n = new ClassNode{c, nullptr};
        if (!head) { head = n; return; }
        ClassNode *p = head;
        while (p->next) p = p->next;              // walk to the last node
        p->next = n;
    }
    bool removeAt(int pos) {                      // DELETE by position (0 = first)
        if (pos < 0 || !head) return false;
        if (pos == 0) { ClassNode *n = head; head = head->next; delete n; return true; }
        ClassNode *prev = head;
        for (int i = 1; i < pos && prev->next; i++) prev = prev->next;
        if (!prev->next) return false;
        ClassNode *n = prev->next;
        prev->next = n->next;                     // skip the deleted node
        delete n;
        return true;
    }
    bool has(int id) const {                      // SEARCH
        for (ClassNode *p = head; p; p = p->next) if (p->data.id == id) return true;
        return false;
    }
    const Cls *clashWith(const Cls &c) const {
        for (ClassNode *p = head; p; p = p->next) if (overlap(p->data, c)) return &p->data;
        return nullptr;
    }
    vector<Cls> toVector() const {                // TRAVERSAL
        vector<Cls> v;
        for (ClassNode *p = head; p; p = p->next) v.push_back(p->data);
        return v;
    }
} mine;

int mineAddById(int id, string &why) {            // 1 = added, 0 = skipped (see why), -1 = id not found
    for (const Cls &c : timetable) {
        if (c.id != id) continue;
        if (mine.has(id)) { why = "Already in your timetable"; return 0; }
        if (const Cls *bad = mine.clashWith(c)) { why = "Clash with " + describe(*bad); return 0; }
        mine.add(c);
        return 1;
    }
    return -1;
}
void restoreMine(const vector<Cls> &saved) {      // rebuild the list from my_timetable.csv
    for (const Cls &s : saved)
        for (const Cls &c : timetable)
            if (joint(c, s) && c.section == s.section) { mine.add(c); break; }
}

// ================================================================ 5. CLASH FINDER + FREE ROOMS
int findClashes(const string &type, vector<pair<Cls, Cls> > &pairs, size_t limit) {
    int count = 0;                                // type: "room", "teacher" or "section"
    for (size_t i = 0; i < timetable.size(); i++)
        for (size_t j = i + 1; j < timetable.size(); j++) {
            const Cls &a = timetable[i], &b = timetable[j];
            if (!overlap(a, b) || joint(a, b) || fieldOf(a, type) != fieldOf(b, type)) continue;
            if ((size_t)++count <= limit) pairs.push_back(make_pair(a, b));
        }
    return count;
}
vector<string> freeRooms(const Cls &slot, int &totalRooms) {
    set<string> all, busy;                        // a set has no repeats and is sorted A-Z
    for (const Cls &c : timetable) { all.insert(c.room); if (overlap(c, slot)) busy.insert(c.room); }
    vector<string> freeList;
    for (const string &r : all) if (!busy.count(r)) freeList.push_back(r);
    totalRooms = (int)all.size();
    return freeList;
}

// ================================================================ 6. BACKTRACKING (course planner)
// Pick ONE section for every course so that no two chosen sections overlap.
struct Option { string course, section; vector<Cls> rows; };      // one section of a course
struct PlanResult {
    long long count = 0, backtracks = 0;          // plans found (max 10000), clashes avoided
    vector<vector<Option> > firstPlans;           // the first 3 plans
    string error;
};
vector<vector<Option> > groups;                   // groups[c] = all sections of course c
vector<Option> picked;                            // sections chosen so far
PlanResult res;

bool optionsClash(const Option &a, const Option &b) {
    for (const Cls &x : a.rows) for (const Cls &y : b.rows) if (overlap(x, y)) return true;
    return false;
}
void plan(size_t level) {                         // recursion: one level = one course
    if (res.count >= 10000) return;
    if (level == groups.size()) {                 // base case: every course has a section
        if (++res.count <= 3) res.firstPlans.push_back(picked);
        return;
    }
    for (const Option &opt : groups[level]) {
        bool bad = false;
        for (const Option &p : picked) if (optionsClash(p, opt)) { bad = true; break; }
        if (bad) { res.backtracks++; continue; }  // clash: try the next section
        picked.push_back(opt);                    // choose
        plan(level + 1);                          // go deeper
        picked.pop_back();                        // undo = backtrack
    }
}
PlanResult planCourses(const vector<string> &names) {
    groups.clear(); picked.clear(); res = PlanResult();
    for (const string &name : names) {
        map<string, Option> bySection;            // group this course's rows by section
        for (const Cls &c : timetable)
            if (lower(c.course) == lower(trim(name))) {
                Option &o = bySection[c.section];
                o.course = c.course; o.section = c.section; o.rows.push_back(c);
            }
        if (bySection.empty()) { res.error = "Course not found: " + name; return res; }
        vector<Option> g;
        for (auto &kv : bySection) g.push_back(kv.second);
        groups.push_back(g);
    }
    if (groups.empty()) { res.error = "Choose at least one course."; return res; }
    plan(0);
    return res;
}

// ================================================================ WEB PART (sockets + JSON)
#include "server.cpp"

// ================================================================ main
int main() {
    if (!loadCSV("timetable.csv", timetable)) {
        cout << "Error: timetable.csv not found. Put it in the same folder as the program.\n";
        return 1;
    }
    for (Cls &c : timetable) c.id = nextId++;                       // give every class an id
    vector<Cls> saved;
    if (loadCSV("my_timetable.csv", saved)) restoreMine(saved);     // load my saved timetable
    cout << "Loaded " << timetable.size() << " classes, " << mine.toVector().size() << " in my timetable.\n";
    return runServer();                                             // start the web page (server.cpp)
}
