# CampusFlow
Timetable manager. The C++ program is a small local web server: it does all the work
(search, sort, clashes, planner, undo) and the browser is only the screen.

## Folders
    CampusFlow/
    ├── src/
    │   ├── main.cpp          all data structure + algorithm logic
    │   └── server.cpp        web part only (sockets, HTTP, JSON); included by main.cpp
    ├── web/
    │   └── campusflow.html   the web page (what you see in the browser)
    ├── data/
    │   ├── timetable.csv     all university classes
    │   └── my_timetable.csv  my saved timetable (created by the program)
    ├── README.md
    └── LICENSE

## Run
Open a terminal **inside the CampusFlow folder** and compile only `src/main.cpp`:

    g++ -std=c++17 -O2 src/main.cpp -o campusflow -lws2_32
    ./campusflow.exe

Then open http://localhost:8080 (it opens by itself on Windows/Mac). Press Ctrl+C to stop.

## DSA used (all in src/main.cpp)
1. Sorting: bubble, merge, quick sort
2. Searching: linear search, binary search
3. Stack (linked list): undo
4. Singly linked list: my timetable
5. Clash finder and free rooms (set)
6. Recursion + backtracking: course planner
