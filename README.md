# IPC_Project

A terminal-based multi-client chat application built using
Features:
-   Multiple clients support
-   Real-time messaging
-   Private messaging (@username)
-   Chat history
-   Online users list
-   Scrollable UI
-   Colored terminal interface


Project Structure

    IPC-Chat/
    ├── server.c
    ├── client.c
    ├── chat_ui_1.py
    └── README.md



Technologies Used

-   C (IPC using FIFO)
-   Python (curses)
-   Multithreading

Compile

    gcc server.c -o server -lpthread
    gcc client.c -o client

Run Server

    ./server
