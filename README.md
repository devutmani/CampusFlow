# CampusFlow
Timetable manager. The C++ program is a small local web server: it does all the work
(search, sort, clashes, planner, undo) and the browser is only the screen.

## Run
Keep `main.cpp`, `campusflow.html` and `timetable.csv` in one folder.

    Linux / Mac : g++ -std=c++17 -O2 main.cpp -o campusflow && ./campusflow
    Windows     : g++ -std=c++17 -O2 main.cpp -o campusflow -lws2_32 && campusflow.exe

Then open http://localhost:8080 (it opens by itself on Windows/Mac). Press Ctrl+C to stop.

## DSA used (all in main.cpp)
Singly linked list (my timetable), stack (undo), linear search, binary search,
bubble / merge / quick sort, recursion, backtracking (planner).
