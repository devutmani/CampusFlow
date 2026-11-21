#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stack>
#include <set>
#include <map>
#include <algorithm>
#include <cctype>
using namespace std;

struct Cls {
    string course, code, teacher, section, room, day;
    int start = 0, end = 0;                     
};

const string DAYS[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const int NDAYS = 6;

string lower(string s) {
    for (char &c : s) c = tolower((unsigned char)c);
    return s;
}

string trim(const string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

int dayNo(string d) {                       
    d = lower(trim(d));
    for (int i = 0; i < NDAYS; i++)
        if (d == lower(DAYS[i]) || (d.size() > 3 && d.substr(0, 3) == lower(DAYS[i]))) return i;
    return -1;
}

string hhmm(int m) {   
    char b[16];
    snprintf(b, sizeof b, "%02d:%02d", m / 60, m % 60);
    return b;
}

bool parseTime(const string &t, int &out) {   
    int h, m;
    char c;
    stringstream ss(t);
    if (!(ss >> h >> c >> m) || c != ':' || h < 0 || h > 23 || m < 0 || m > 59) return false;
    out = h * 60 + m;
    return true;
}

string ask(const string &prompt) {
    cout << prompt;
    string s;
    if (!getline(cin, s)) { cout << "\nBye!\n"; exit(0); }
    return trim(s);
}

int askInt(const string &prompt, int lo, int hi) {
    while (true) {
        string s = ask(prompt);
        try {
            size_t used;
            int v = stoi(s, &used);
            if (used == s.size() && v >= lo && v <= hi) return v;
        } catch (...) {}
        cout << "Please enter a number from " << lo << " to " << hi << ".\n";
    }
}

void show(const Cls &c) {
    cout << c.course << " | " << c.teacher << " | " << c.section << " | " << c.room
         << " | " << c.day << " " << hhmm(c.start) << "-" << hhmm(c.end) << "\n";
}

bool sameClass(const Cls &a, const Cls &b) {
    return a.course == b.course && a.teacher == b.teacher && a.section == b.section &&
           a.room == b.room && a.day == b.day && a.start == b.start && a.end == b.end;
}

bool overlap(const Cls &a, const Cls &b) {
    return a.day == b.day && a.start < b.end && b.start < a.end;
}

bool joint(const Cls &a, const Cls &b) {
    return a.course == b.course && a.teacher == b.teacher && a.room == b.room &&
           a.day == b.day && a.start == b.start && a.end == b.end;
}

bool loadCSV(const string &file, vector<Cls> &out) {
    ifstream in(file);
    if (!in) return false;
    string line;
    getline(in, line);                                   
    int n = 1;
    while (getline(in, line)) {
        n++;
        if (trim(line).empty()) continue;
        vector<string> f;
        stringstream ss(line);
        string x;
        while (getline(ss, x, ',')) f.push_back(trim(x));
        Cls c;
        bool ok = f.size() >= 8;
        if (ok) {
            c = {f[0], f[1], f[2], f[3], f[4], "", 0, 0};
            int d = dayNo(f[5]);
            ok = d >= 0 && parseTime(f[6], c.start) && parseTime(f[7], c.end) &&
                 c.start < c.end && !c.course.empty() && !c.section.empty();
            if (ok) c.day = DAYS[d];
        }
        if (ok) out.push_back(c);
        else cout << "Skipped bad row " << n << ": " << line << "\n";
    }
    return true;
}

bool saveCSV(const string &file, const vector<Cls> &v) {
    ofstream out(file);
    if (!out) return false;
    out << "Course,Code,Teacher,Section,Room,Day,Start,End\n";
    for (const Cls &c : v)
        out << c.course << ',' << c.code << ',' << c.teacher << ',' << c.section << ','
            << c.room << ',' << c.day << ',' << hhmm(c.start) << ',' << hhmm(c.end) << '\n';
    return true;
}

long long compares = 0;
int sortKey = 1;

bool before(const Cls &a, const Cls &b) {
    compares++;
    if (sortKey == 2 && a.course != b.course) return lower(a.course) < lower(b.course);
    if (sortKey == 3 && a.teacher != b.teacher) return lower(a.teacher) < lower(b.teacher);
    if (sortKey == 4 && a.room != b.room) return a.room < b.room;
    if (dayNo(a.day) != dayNo(b.day)) return dayNo(a.day) < dayNo(b.day);
    return a.start < b.start;
}

void bubbleSort(vector<Cls> &v) {
    for (size_t i = 0; i + 1 < v.size(); i++) {
        bool swapped = false;
        for (size_t j = 0; j + 1 < v.size() - i; j++)
            if (before(v[j + 1], v[j])) { swap(v[j], v[j + 1]); swapped = true; }
        if (!swapped) break;                                 // already sorted
    }
}

void mergeSort(vector<Cls> &v, int l, int r) {           
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

void quickSort(vector<Cls> &v, int lo, int hi) {          
    if (lo >= hi) return;
    swap(v[(lo + hi) / 2], v[hi]);                      
    int p = lo;
    for (int i = lo; i < hi; i++)
        if (before(v[i], v[hi])) swap(v[i], v[p++]);
    swap(v[p], v[hi]);
    quickSort(v, lo, p - 1);
    quickSort(v, p + 1, hi);
}

void sortMenu(vector<Cls> &v) {
    cout << "Sort by: 1.Day & time  2.Course  3.Teacher  4.Room\n";
    sortKey = askInt("Choice: ", 1, 4);
    cout << "Algorithm: 1.Bubble  2.Merge  3.Quick\n";
    int algo = askInt("Choice: ", 1, 3);
    compares = 0;
    if (algo == 1) bubbleSort(v);
    else if (algo == 2) mergeSort(v, 0, (int)v.size());
    else quickSort(v, 0, (int)v.size() - 1);
    cout << "Sorted " << v.size() << " classes using " << compares << " comparisons.\n";
}

vector<int> linearSearch(const vector<Cls> &v, int field, const string &text) {
    vector<int> hits;
    string t = lower(text);
    for (size_t i = 0; i < v.size(); i++) {
        const Cls &c = v[i];
        const string &f = field == 1 ? c.section : field == 2 ? c.teacher :
                          field == 3 ? c.course : field == 4 ? c.room : c.day;
        if (lower(f).find(t) != string::npos) hits.push_back((int)i);
    }
    return hits;
}

void searchMenu(const vector<Cls> &v) {
    cout << "Search by: 1.Section  2.Teacher  3.Course  4.Room  5.Day\n";
    int f = askInt("Choice: ", 1, 5);
    string text = ask("Text: ");
    vector<int> hits = linearSearch(v, f, text);
    for (int i : hits) show(v[i]);
    cout << hits.size() << " class(es) found.\n";
}

void binarySearchMenu(const vector<Cls> &v) {
    vector<Cls> s = v;
    sortKey = 2;
    mergeSort(s, 0, (int)s.size());
    string want = lower(ask("Exact course name: "));
    int lo = 0, hi = (int)s.size() - 1, steps = 0, found = -1;
    while (lo <= hi) {
        steps++;
        int mid = (lo + hi) / 2;
        string name = lower(s[mid].course);
        if (name == want) { found = mid; break; }
        if (name < want) lo = mid + 1; else hi = mid - 1;
    }
    if (found < 0) { cout << "Not found (" << steps << " steps).\n"; return; }
    while (found > 0 && lower(s[found - 1].course) == want) found--;   // first match
    cout << "Found after " << steps << " steps:\n";
    for (size_t i = found; i < s.size() && lower(s[i].course) == want; i++) show(s[i]);
}

int findConflict(const vector<Cls> &v, const Cls &c) {
    for (size_t i = 0; i < v.size(); i++)
        if (overlap(v[i], c) && !joint(v[i], c) &&
            (v[i].room == c.room || v[i].teacher == c.teacher || v[i].section == c.section))
            return (int)i;
    return -1;
}

void clashReport(const vector<Cls> &v) {
    int count = 0;
    for (size_t i = 0; i < v.size(); i++)
        for (size_t j = i + 1; j < v.size(); j++) {
            if (!overlap(v[i], v[j]) || joint(v[i], v[j])) continue;
            string why = v[i].room == v[j].room ? "ROOM " + v[i].room :
                         v[i].teacher == v[j].teacher ? "TEACHER " + v[i].teacher :
                         v[i].section == v[j].section ? "SECTION " + v[i].section : "";
            if (why.empty()) continue;
            if (++count <= 30) {
                cout << "[" << why << "]\n  ";
                show(v[i]);
                cout << "  ";
                show(v[j]);
            }
        }
    cout << count << " clash(es) found" << (count > 30 ? " (showing first 30)" : "") << ".\n";
}

void freeRooms(const vector<Cls> &v) {
    int d = dayNo(ask("Day (Mon-Sat): "));
    Cls slot;
    if (d < 0 || !parseTime(ask("Start (HH:MM): "), slot.start) ||
        !parseTime(ask("End (HH:MM): "), slot.end) || slot.start >= slot.end) {
        cout << "Invalid day or time.\n";
        return;
    }
    slot.day = DAYS[d];
    set<string> all, busy;
    for (const Cls &c : v) {
        all.insert(c.room);
        if (overlap(c, slot)) busy.insert(c.room);
    }
    int n = 0;
    for (const string &r : all)
        if (!busy.count(r)) { cout << r << "\n"; n++; }
    cout << n << " of " << all.size() << " rooms are free.\n";
}

struct Action { bool wasAdd; Cls rec; };   
stack<Action> undoStack;

void addClass(vector<Cls> &v) {
    Cls c;
    c.course = ask("Course: ");
    c.code = ask("Code: ");
    c.teacher = ask("Teacher: ");
    c.section = ask("Section: ");
    c.room = ask("Room: ");
    int d = dayNo(ask("Day (Mon-Sat): "));
    bool ok = parseTime(ask("Start (HH:MM): "), c.start) && parseTime(ask("End (HH:MM): "), c.end);
    for (const string *s : {&c.course, &c.code, &c.teacher, &c.section, &c.room})
        if (s->empty() || s->find(',') != string::npos) ok = false;
    if (!ok || d < 0 || c.start >= c.end) {
        cout << "Invalid input (no empty fields, no commas, valid day and times).\n";
        return;
    }
    c.day = DAYS[d];
    int bad = findConflict(v, c);
    if (bad >= 0) { cout << "Clash with: "; show(v[bad]); return; }
    v.push_back(c);
    undoStack.push({true, c});
    cout << "Class added.\n";
}

void removeClass(vector<Cls> &v) {
    vector<int> hits = linearSearch(v, 3, ask("Part of the course name: "));
    if (hits.empty()) { cout << "No match.\n"; return; }
    int shown = min((int)hits.size(), 20);
    for (int i = 0; i < shown; i++) { cout << i + 1 << ". "; show(v[hits[i]]); }
    int n = askInt("Remove which number (0 = cancel): ", 0, shown);
    if (n == 0) return;
    undoStack.push({false, v[hits[n - 1]]});
    v.erase(v.begin() + hits[n - 1]);
    cout << "Removed.\n";
}

void undo(vector<Cls> &v) {
    if (undoStack.empty()) { cout << "Nothing to undo.\n"; return; }
    Action a = undoStack.top();
    undoStack.pop();
    if (a.wasAdd) {                                      
        for (size_t i = v.size(); i-- > 0;)
            if (sameClass(v[i], a.rec)) { v.erase(v.begin() + i); break; }
        cout << "Undone: the added class was removed.\n";
    } else {                                             
        v.push_back(a.rec);
        cout << "Undone: the removed class was restored.\n";
    }
}

struct Node { Cls data; Node *next; };

class MyTimetable {
    Node *head = nullptr;
public:
    ~MyTimetable() { while (head) { Node *n = head; head = head->next; delete n; } }
    void add(const Cls &c) {                             
        Node *n = new Node{c, nullptr};
        if (!head) { head = n; return; }
        Node *cur = head;
        while (cur->next) cur = cur->next;
        cur->next = n;
    }
    bool removeAt(int pos) {                             
        if (pos < 1 || !head) return false;
        if (pos == 1) { Node *n = head; head = head->next; delete n; return true; }
        Node *prev = head;
        for (int i = 2; i < pos && prev->next; i++) prev = prev->next;
        if (!prev->next) return false;
        Node *n = prev->next;
        prev->next = n->next;
        delete n;
        return true;
    }
    int clashWith(const Cls &c) const {                     
        int i = 1;
        for (Node *p = head; p; p = p->next, i++)
            if (overlap(p->data, c)) return i;
        return -1;
    }
    vector<Cls> toVector() const {
        vector<Cls> v;
        for (Node *p = head; p; p = p->next) v.push_back(p->data);
        return v;
    }
    void print() const {
        int i = 1;
        for (Node *p = head; p; p = p->next, i++) { cout << i << ". "; show(p->data); }
        if (!head) cout << "Your timetable is empty.\n";
    }
};

void myMenu(const vector<Cls> &v, MyTimetable &mine) {
    while (true) {
        cout << "\n-- My timetable --\n1.Show  2.Add a class  3.Remove  4.Save  0.Back\n";
        int c = askInt("Choice: ", 0, 4);
        if (c == 0) return;
        if (c == 1) {
            vector<Cls> mv = mine.toVector();
            sortKey = 1;
            sort(mv.begin(), mv.end(), [](const Cls &a, const Cls &b) { return before(a, b); });
            MyTimetable tmp;
            for (const Cls &x : mv) tmp.add(x);
            tmp.print();
        } else if (c == 2) {
            vector<int> hits = linearSearch(v, 3, ask("Part of the course name: "));
            int shown = min((int)hits.size(), 20);
            if (!shown) { cout << "No match.\n"; continue; }
            for (int i = 0; i < shown; i++) { cout << i + 1 << ". "; show(v[hits[i]]); }
            int n = askInt("Add which number (0 = cancel): ", 0, shown);
            if (n == 0) continue;
            const Cls &pick = v[hits[n - 1]];
            int bad = mine.clashWith(pick);
            if (bad > 0) cout << "Clashes with your class number " << bad << ".\n";
            else { mine.add(pick); cout << "Added.\n"; }
        } else if (c == 3) {
            mine.print();
            int n = askInt("Remove which number (0 = cancel): ", 0, 100000);
            if (n > 0) cout << (mine.removeAt(n) ? "Removed.\n" : "No such number.\n");
        } else {
            cout << (saveCSV("my_timetable.csv", mine.toVector()) ? "Saved to my_timetable.csv\n"
                                                                  : "Could not save.\n");
        }
    }
}

struct Option { string course, section; vector<Cls> rows; };

long long plans = 0, backtracks = 0;
vector<vector<Option>> groups;
vector<Option> picked;

bool optionsClash(const Option &a, const Option &b) {
    for (const Cls &x : a.rows)
        for (const Cls &y : b.rows)
            if (overlap(x, y)) return true;
    return false;
}

void plan(size_t level) {                                  
    if (plans >= 10000) return;
    if (level == groups.size()) {                          
        if (++plans <= 3) {
            cout << "Plan " << plans << ":\n";
            for (const Option &o : picked) {
                cout << "  " << o.course << " - " << o.section << " (";
                for (size_t i = 0; i < o.rows.size(); i++)
                    cout << (i ? ", " : "") << o.rows[i].day << " " << hhmm(o.rows[i].start);
                cout << ")\n";
            }
        }
        return;
    }
    for (const Option &opt : groups[level]) {
        bool bad = false;
        for (const Option &p : picked) if (optionsClash(p, opt)) { bad = true; break; }
        if (bad) { backtracks++; continue; }            
        picked.push_back(opt);                          
        plan(level + 1);                                
        picked.pop_back();                              
    }
}

void plannerMenu(const vector<Cls> &v) {
    groups.clear();
    picked.clear();
    plans = backtracks = 0;
    cout << "Enter up to 6 course names (empty line to finish).\n";
    while (groups.size() < 6) {
        string name = ask("Course: ");
        if (name.empty()) break;
        vector<int> hits = linearSearch(v, 3, name);
        if (hits.empty()) { cout << "  not found\n"; continue; }
        string exact = v[hits[0]].course;                  
        for (int i : hits) if (lower(v[i].course) == lower(name)) { exact = v[i].course; break; }
        map<string, Option> bySection;
        for (const Cls &c : v)
            if (c.course == exact) {
                Option &o = bySection[c.section];
                o.course = exact; o.section = c.section; o.rows.push_back(c);
            }
        vector<Option> g;
        for (auto &kv : bySection) g.push_back(kv.second);
        groups.push_back(g);
        cout << "  added: " << exact << " (" << g.size() << " sections)\n";
    }
    if (groups.empty()) return;
    plan(0);
    cout << (plans >= 10000 ? "10000+" : to_string(plans)) << " clash-free plan(s), "
         << backtracks << " backtracks.\n";
}

int main() {
    vector<Cls> timetable;
    if (!loadCSV("timetable.csv", timetable)) {
        cout << "Error: timetable.csv not found. Put it next to the program.\n";
        return 1;
    }
    cout << "Loaded " << timetable.size() << " classes.\n";

    MyTimetable mine;
    vector<Cls> saved;
    if (loadCSV("my_timetable.csv", saved)) for (const Cls &c : saved) mine.add(c);

    while (true) {
        cout << "\n===== CampusFlow =====\n"
             << " 1. Show all classes        7. Clash report\n"
             << " 2. Search (linear)         8. Free classroom finder\n"
             << " 3. Sort                    9. Find course (binary search)\n"
             << " 4. Add a class            10. My timetable (linked list)\n"
             << " 5. Remove a class         11. Course planner (backtracking)\n"
             << " 6. Undo (" << undoStack.size() << ")               12. Save to timetable.csv\n"
             << " 0. Exit\n";
        switch (askInt("Choice: ", 0, 12)) {
            case 1: for (const Cls &c : timetable) show(c); break;
            case 2: searchMenu(timetable); break;
            case 3: sortMenu(timetable); break;
            case 4: addClass(timetable); break;
            case 5: removeClass(timetable); break;
            case 6: undo(timetable); break;
            case 7: clashReport(timetable); break;
            case 8: freeRooms(timetable); break;
            case 9: binarySearchMenu(timetable); break;
            case 10: myMenu(timetable, mine); break;
            case 11: plannerMenu(timetable); break;
            case 12: cout << (saveCSV("timetable.csv", timetable) ? "Saved.\n" : "Could not save.\n"); break;
            case 0: cout << "Goodbye!\n"; return 0;
        }
    }
}